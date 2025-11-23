/*
Project: AR Image Processing - C implementations
Files included in this single document (separated by comments):
  1) baseline.c      - naive 3-loop convolution + sobel (single-threaded)
  2) optimized.c     - optimized pointer arithmetic / reduced bounds checks
  3) openmp.c        - optimized + OpenMP pragmas (multi-threaded)

Compile examples:
  gcc -O2 -std=c11 -o baseline baseline.c
  gcc -O2 -std=c11 -o optimized optimized.c
  gcc -O2 -std=c11 -fopenmp -o openmp openmp.c

Note: all three files implement BMP load/save for 8-bit or 24-bit inputs (they convert color -> gray).
*/

// ---------------------- baseline.c ----------------------

/* baseline.c
   Naive convolution and Sobel on grayscale BMP.
   Usage: ./baseline input.bmp out_conv.bmp out_sobel.bmp
*/

#define _POSIX_C_SOURCE 199309L   /* 啟用 clock_gettime / CLOCK_MONOTONIC */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#pragma pack(push,1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;
typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

static double now_sec() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec*1e-9;
}

unsigned char* load_gray_bmp_baseline(const char* fname, int* w, int* h) {
    FILE* f = fopen(fname,"rb");
    if(!f) { perror("fopen"); return NULL;}
    BITMAPFILEHEADER bf;
    BITMAPINFOHEADER bi;
    if(fread(&bf, sizeof(bf),1,f)!=1){ fclose(f); fprintf(stderr,"Read bf failed\n"); return NULL; }
    if(fread(&bi, sizeof(bi),1,f)!=1){ fclose(f); fprintf(stderr,"Read bi failed\n"); return NULL; }
    if(bf.bfType != 0x4D42) { fclose(f); fprintf(stderr,"Not BMP\n"); return NULL;}
    if(bi.biBitCount != 8 && bi.biBitCount != 24) {
        fclose(f); fprintf(stderr,"Only 8bit or 24bit supported\n"); return NULL;
    }
    *w = bi.biWidth; *h = abs(bi.biHeight);
    int rowbytes_in = ((bi.biBitCount * (*w) + 31)/32)*4;
    unsigned char* img = malloc((*w)*(*h));
    if(!img){ fclose(f); fprintf(stderr,"malloc failed\n"); return NULL; }
    fseek(f, bf.bfOffBits, SEEK_SET);
    if(bi.biBitCount == 8) {
        // assume palette present; read rows
        for(int y=(*h)-1;y>=0;--y){
            unsigned char* row = img + y*(*w);
            fread(row,1,*w,f);
            int pad = rowbytes_in - (*w);
            if(pad) fseek(f,pad,SEEK_CUR);
        }
    } else {
        // 24-bit, convert to gray
        for(int y=(*h)-1;y>=0;--y){
            unsigned char* row = img + y*(*w);
            for(int x=0;x<*w;++x){
                unsigned char bgr[3];
                fread(bgr,1,3,f);
                int gray = (int)(0.299*bgr[2] + 0.587*bgr[1] + 0.114*bgr[0]);
                row[x] = (unsigned char)gray;
            }
            int pad = rowbytes_in - (*w)*3;
            if(pad) fseek(f,pad,SEEK_CUR);
        }
    }
    fclose(f);
    return img;
}

void save_gray_bmp_baseline(const char* fname, unsigned char* img, int w, int h) {
    int rowbytes = ((w + 3)/4)*4;
    int imgsize = rowbytes*h;
    BITMAPFILEHEADER bf = {0x4D42, (uint32_t)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4+imgsize), 0,0, (uint32_t)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4)};
    BITMAPINFOHEADER bi = {40, w, h, 1, 8, 0, (uint32_t)imgsize, 3780,3780,256,0};
    FILE* f = fopen(fname,"wb");
    fwrite(&bf,sizeof(bf),1,f);
    fwrite(&bi,sizeof(bi),1,f);
    for(int i=0;i<256;i++){ unsigned char pal[4] = { (unsigned char)i,(unsigned char)i,(unsigned char)i,0 }; fwrite(pal,1,4,f); }
    unsigned char* pad = calloc(1, rowbytes - w);
    for(int y=h-1;y>=0;--y){ fwrite(img + y*w,1,w,f); if(rowbytes>w) fwrite(pad,1,rowbytes-w,f); }
    free(pad);
    fclose(f);
}

void conv3x3_baseline(unsigned char* in, unsigned char* out, int w, int h, const int k[3][3], int kdiv) {
    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            int sum = 0;
            for(int j=-1;j<=1;j++){
                for(int i=-1;i<=1;i++){
                    int xx = x+i, yy = y+j;
                    if(xx<0) xx=0; if(xx>=w) xx=w-1;
                    if(yy<0) yy=0; if(yy>=h) yy=h-1;
                    sum += in[yy*w + xx] * k[j+1][i+1];
                }
            }
            int v = sum / kdiv;
            if(v<0) v=0; if(v>255) v=255;
            out[y*w + x] = (unsigned char)v;
        }
    }
}

void sobel_baseline(unsigned char* in, unsigned char* out, int w, int h) {
    int gx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    int gy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            int sx=0, sy=0;
            for(int j=-1;j<=1;j++){
                for(int i=-1;i<=1;i++){
                    int xx = x+i, yy = y+j;
                    if(xx<0) xx=0; if(xx>=w) xx=w-1;
                    if(yy<0) yy=0; if(yy>=h) yy=h-1;
                    int v = in[yy*w + xx];
                    sx += v * gx[j+1][i+1];
                    sy += v * gy[j+1][i+1];
                }
            }
            int mag = abs(sx) + abs(sy);
            if(mag>255) mag=255;
            out[y*w + x] = (unsigned char)mag;
        }
    }
}

int main_baseline(int argc, char** argv){
    if(argc<4){ fprintf(stderr,"Usage: %s input.bmp out_conv.bmp out_sobel.bmp\n", argv[0]); return 1;}
    int w,h;
    unsigned char* in = load_gray_bmp_baseline(argv[1], &w, &h);
    if(!in) return 1;
    unsigned char* buf = malloc(w*h);
    unsigned char* buf2 = malloc(w*h);
    double t0 = now_sec();
    int blurk[3][3] = {{1,1,1},{1,1,1},{1,1,1}};
    conv3x3_baseline(in, buf, w, h, blurk, 9);
    double t1 = now_sec();
    sobel_baseline(in, buf2, w, h);
    double t2 = now_sec();
    save_gray_bmp_baseline(argv[2], buf, w, h);
    save_gray_bmp_baseline(argv[3], buf2, w, h);
    printf("conv_time=%.6f sobel_time=%.6f\n", t1-t0, t2-t1);
    free(in); free(buf); free(buf2);
    return 0;
}

// If compiling baseline as standalone, provide main alias
#ifdef BUILD_BASELINE_MAIN
int main(int argc, char** argv){ return main_baseline(argc, argv); }
#endif


// ---------------------- optimized.c ----------------------

/* optimized.c
   Cache-friendly convolution and Sobel using pointer arithmetic and reduced branch checks.
   Usage: ./optimized input.bmp out_conv.bmp out_sobel.bmp
*/

/* Reuse the same headers and structures */

static double now_sec_opt() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec*1e-9;
}

unsigned char* load_gray_bmp_opt(const char* fname, int* w, int* h) {
    FILE* f = fopen(fname,"rb");
    if(!f) { perror("fopen"); return NULL;}
    BITMAPFILEHEADER bf;
    BITMAPINFOHEADER bi;
    if(fread(&bf, sizeof(bf),1,f)!=1){ fclose(f); fprintf(stderr,"Read bf failed\n"); return NULL; }
    if(fread(&bi, sizeof(bi),1,f)!=1){ fclose(f); fprintf(stderr,"Read bi failed\n"); return NULL; }
    if(bf.bfType != 0x4D42) { fclose(f); fprintf(stderr,"Not BMP\n"); return NULL;}
    if(bi.biBitCount != 8 && bi.biBitCount != 24) {
        fclose(f); fprintf(stderr,"Only 8bit or 24bit supported\n"); return NULL;
    }
    *w = bi.biWidth; *h = abs(bi.biHeight);
    int rowbytes_in = ((bi.biBitCount * (*w) + 31)/32)*4;
    unsigned char* img = malloc((*w)*(*h));
    if(!img){ fclose(f); fprintf(stderr,"malloc failed\n"); return NULL; }
    fseek(f, bf.bfOffBits, SEEK_SET);
    if(bi.biBitCount == 8) {
        for(int y=(*h)-1;y>=0;--y){
            unsigned char* row = img + y*(*w);
            fread(row,1,*w,f);
            int pad = rowbytes_in - (*w);
            if(pad) fseek(f,pad,SEEK_CUR);
        }
    } else {
        for(int y=(*h)-1;y>=0;--y){
            unsigned char* row = img + y*(*w);
            for(int x=0;x<*w;++x){
                unsigned char bgr[3];
                fread(bgr,1,3,f);
                int gray = (int)(0.299*bgr[2] + 0.587*bgr[1] + 0.114*bgr[0]);
                row[x] = (unsigned char)gray;
            }
            int pad = rowbytes_in - (*w)*3;
            if(pad) fseek(f,pad,SEEK_CUR);
        }
    }
    fclose(f);
    return img;
}

void save_gray_bmp_opt(const char* fname, unsigned char* img, int w, int h) {
    int rowbytes = ((w + 3)/4)*4;
    int imgsize = rowbytes*h;
    BITMAPFILEHEADER bf = {0x4D42, (uint32_t)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4+imgsize), 0,0, (uint32_t)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4)};
    BITMAPINFOHEADER bi = {40, w, h, 1, 8, 0, (uint32_t)imgsize, 3780,3780,256,0};
    FILE* f = fopen(fname,"wb");
    fwrite(&bf,sizeof(bf),1,f);
    fwrite(&bi,sizeof(bi),1,f);
    for(int i=0;i<256;i++){ unsigned char pal[4] = { (unsigned char)i,(unsigned char)i,(unsigned char)i,0 }; fwrite(pal,1,4,f); }
    unsigned char* pad = calloc(1, rowbytes - w);
    for(int y=h-1;y>=0;--y){ fwrite(img + y*w,1,w,f); if(rowbytes>w) fwrite(pad,1,rowbytes-w,f); }
    free(pad);
    fclose(f);
}

void conv3x3_opt(unsigned char* in, unsigned char* out, int w, int h, const int k[3][3], int kdiv) {
    // interior pixels: no bounds checks
    for(int y=1;y<h-1;y++){
        int yw = y*w;
        for(int x=1;x<w-1;x++){
            int idx = yw + x;
            int sum = 0;
            sum += in[idx - w - 1] * k[0][0];
            sum += in[idx - w    ] * k[0][1];
            sum += in[idx - w + 1] * k[0][2];
            sum += in[idx - 1    ] * k[1][0];
            sum += in[idx        ] * k[1][1];
            sum += in[idx + 1    ] * k[1][2];
            sum += in[idx + w - 1] * k[2][0];
            sum += in[idx + w    ] * k[2][1];
            sum += in[idx + w + 1] * k[2][2];
            int v = sum / kdiv;
            if(v<0) v=0; if(v>255) v=255;
            out[idx] = (unsigned char)v;
        }
    }
    // borders: copy from input
    for(int x=0;x<w;x++){ out[x]=in[x]; out[(h-1)*w + x] = in[(h-1)*w + x]; }
    for(int y=0;y<h;y++){ out[y*w] = in[y*w]; out[y*w + (w-1)] = in[y*w + (w-1)]; }
}

void sobel_opt(unsigned char* in, unsigned char* out, int w, int h) {
    for(int y=1;y<h-1;y++){
        int yw = y*w;
        for(int x=1;x<w-1;x++){
            int idx = yw + x;
            int p00 = in[idx - w - 1], p01 = in[idx - w], p02 = in[idx - w + 1];
            int p10 = in[idx - 1],     p11 = in[idx],     p12 = in[idx + 1];
            int p20 = in[idx + w - 1], p21 = in[idx + w], p22 = in[idx + w + 1];
            int sx = -p00 + p02 -2*p10 + 2*p12 -p20 + p22;
            int sy = -p00 -2*p01 -p02 + p20 +2*p21 + p22;
            int mag = abs(sx) + abs(sy);
            if(mag>255) mag=255;
            out[idx] = (unsigned char)mag;
        }
    }
    for(int x=0;x<w;x++){ out[x]=0; out[(h-1)*w + x] = 0; }
    for(int y=0;y<h;y++){ out[y*w] = 0; out[y*w + (w-1)] = 0; }
}

int main_optimized(int argc, char** argv){
    if(argc<4){ fprintf(stderr,"Usage: %s input.bmp out_conv.bmp out_sobel.bmp\n", argv[0]); return 1;}
    int w,h;
    unsigned char* in = load_gray_bmp_opt(argv[1], &w, &h);
    if(!in) return 1;
    unsigned char* buf_conv = malloc(w*h);
    unsigned char* buf_sobel = malloc(w*h);
    if(!buf_conv || !buf_sobel){ fprintf(stderr,"malloc failed\n"); free(in); return 1; }
    double t0 = now_sec_opt();
    int blurk[3][3] = {{1,1,1},{1,1,1},{1,1,1}};
    conv3x3_opt(in, buf_conv, w, h, blurk, 9);
    double t1 = now_sec_opt();
    sobel_opt(in, buf_sobel, w, h);
    double t2 = now_sec_opt();
    save_gray_bmp_opt(argv[2], buf_conv, w, h);
    save_gray_bmp_opt(argv[3], buf_sobel, w, h);
    printf("conv_time=%.6f sobel_time=%.6f\n", t1-t0, t2-t1);
    free(in); free(buf_conv); free(buf_sobel);
    return 0;
}

#ifdef BUILD_OPTIMIZED_MAIN
int main(int argc, char** argv){ return main_optimized(argc, argv); }
#endif


// ---------------------- openmp.c ----------------------

/* openmp.c
   Optimized implementation with OpenMP pragmas to parallelize the outer loops.
   Usage: gcc -O2 -fopenmp -std=c11 -o openmp openmp.c
          ./openmp input.bmp out_conv.bmp out_sobel.bmp
*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#pragma pack(push,1)
// reuse BITMAPFILEHEADER and BITMAPINFOHEADER definitions from above
#pragma pack(pop)

static double now_sec_omp() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec*1e-9;
}

// We'll reimplement load/save (same as optimized)
unsigned char* load_gray_bmp_omp(const char* fname, int* w, int* h) {
    FILE* f = fopen(fname,"rb");
    if(!f) { perror("fopen"); return NULL;}
    BITMAPFILEHEADER bf;
    BITMAPINFOHEADER bi;
    if(fread(&bf, sizeof(bf),1,f)!=1){ fclose(f); fprintf(stderr,"Read bf failed\n"); return NULL; }
    if(fread(&bi, sizeof(bi),1,f)!=1){ fclose(f); fprintf(stderr,"Read bi failed\n"); return NULL; }
    if(bf.bfType != 0x4D42) { fclose(f); fprintf(stderr,"Not BMP\n"); return NULL;}
    if(bi.biBitCount != 8 && bi.biBitCount != 24) {
        fclose(f); fprintf(stderr,"Only 8bit or 24bit supported\n"); return NULL;
    }
    *w = bi.biWidth; *h = abs(bi.biHeight);
    int rowbytes_in = ((bi.biBitCount * (*w) + 31)/32)*4;
    unsigned char* img = malloc((*w)*(*h));
    if(!img){ fclose(f); fprintf(stderr,"malloc failed\n"); return NULL; }
    fseek(f, bf.bfOffBits, SEEK_SET);
    if(bi.biBitCount == 8) {
        for(int y=(*h)-1;y>=0;--y){
            unsigned char* row = img + y*(*w);
            fread(row,1,*w,f);
            int pad = rowbytes_in - (*w);
            if(pad) fseek(f,pad,SEEK_CUR);
        }
    } else {
        for(int y=(*h)-1;y>=0;--y){
            unsigned char* row = img + y*(*w);
            for(int x=0;x<*w;++x){
                unsigned char bgr[3];
                fread(bgr,1,3,f);
                int gray = (int)(0.299*bgr[2] + 0.587*bgr[1] + 0.114*bgr[0]);
                row[x] = (unsigned char)gray;
            }
            int pad = rowbytes_in - (*w)*3;
            if(pad) fseek(f,pad,SEEK_CUR);
        }
    }
    fclose(f);
    return img;
}

void save_gray_bmp_omp(const char* fname, unsigned char* img, int w, int h) {
    int rowbytes = ((w + 3)/4)*4;
    int imgsize = rowbytes*h;
    BITMAPFILEHEADER bf = {0x4D42, (uint32_t)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4+imgsize), 0,0, (uint32_t)(sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4)};
    BITMAPINFOHEADER bi = {40, w, h, 1, 8, 0, (uint32_t)imgsize, 3780,3780,256,0};
    FILE* f = fopen(fname,"wb");
    fwrite(&bf,sizeof(bf),1,f);
    fwrite(&bi,sizeof(bi),1,f);
    for(int i=0;i<256;i++){ unsigned char pal[4] = { (unsigned char)i,(unsigned char)i,(unsigned char)i,0 }; fwrite(pal,1,4,f); }
    unsigned char* pad = calloc(1, rowbytes - w);
    for(int y=h-1;y>=0;--y){ fwrite(img + y*w,1,w,f); if(rowbytes>w) fwrite(pad,1,rowbytes-w,f); }
    free(pad);
    fclose(f);
}

void conv3x3_omp(unsigned char* in, unsigned char* out, int w, int h, const int k[3][3], int kdiv) {
    // parallelize outer loop (rows)
    #pragma omp parallel for schedule(static)
    for(int y=1;y<h-1;y++){
        int yw = y*w;
        for(int x=1;x<w-1;x++){
            int idx = yw + x;
            int sum = 0;
            sum += in[idx - w - 1] * k[0][0];
            sum += in[idx - w    ] * k[0][1];
            sum += in[idx - w + 1] * k[0][2];
            sum += in[idx - 1    ] * k[1][0];
            sum += in[idx        ] * k[1][1];
            sum += in[idx + 1    ] * k[1][2];
            sum += in[idx + w - 1] * k[2][0];
            sum += in[idx + w    ] * k[2][1];
            sum += in[idx + w + 1] * k[2][2];
            int v = sum / kdiv;
            if(v<0) v=0; if(v>255) v=255;
            out[idx] = (unsigned char)v;
        }
    }
    // borders: copy from input (not parallel)
    for(int x=0;x<w;x++){ out[x]=in[x]; out[(h-1)*w + x] = in[(h-1)*w + x]; }
    for(int y=0;y<h;y++){ out[y*w] = in[y*w]; out[y*w + (w-1)] = in[y*w + (w-1)]; }
}

void sobel_omp(unsigned char* in, unsigned char* out, int w, int h) {
    #pragma omp parallel for schedule(static)
    for(int y=1;y<h-1;y++){
        int yw = y*w;
        for(int x=1;x<w-1;x++){
            int idx = yw + x;
            int p00 = in[idx - w - 1], p01 = in[idx - w], p02 = in[idx - w + 1];
            int p10 = in[idx - 1],     p11 = in[idx],     p12 = in[idx + 1];
            int p20 = in[idx + w - 1], p21 = in[idx + w], p22 = in[idx + w + 1];
            int sx = -p00 + p02 -2*p10 + 2*p12 -p20 + p22;
            int sy = -p00 -2*p01 -p02 + p20 +2*p21 + p22;
            int mag = abs(sx) + abs(sy);
            if(mag>255) mag=255;
            out[idx] = (unsigned char)mag;
        }
    }
    for(int x=0;x<w;x++){ out[x]=0; out[(h-1)*w + x] = 0; }
    for(int y=0;y<h;y++){ out[y*w] = 0; out[y*w + (w-1)] = 0; }
}

int main_openmp(int argc, char** argv){
    if(argc<4){ fprintf(stderr,"Usage: %s input.bmp out_conv.bmp out_sobel.bmp\n", argv[0]); return 1;}
    int w,h;
    unsigned char* in = load_gray_bmp_omp(argv[1], &w, &h);
    if(!in) return 1;
    unsigned char* buf_conv = malloc(w*h);
    unsigned char* buf_sobel = malloc(w*h);
    if(!buf_conv || !buf_sobel){ fprintf(stderr,"malloc failed\n"); free(in); return 1; }
    double t0 = now_sec_omp();
    int blurk[3][3] = {{1,1,1},{1,1,1},{1,1,1}};
    conv3x3_omp(in, buf_conv, w, h, blurk, 9);
    double t1 = now_sec_omp();
    sobel_omp(in, buf_sobel, w, h);
    double t2 = now_sec_omp();
    save_gray_bmp_omp(argv[2], buf_conv, w, h);
    save_gray_bmp_omp(argv[3], buf_sobel, w, h);
    printf("conv_time=%.6f sobel_time=%.6f\n", t1-t0, t2-t1);
    free(in); free(buf_conv); free(buf_sobel);
    return 0;
}

#ifdef BUILD_OPENMP_MAIN
int main(int argc, char** argv){ return main_openmp(argc, argv); }
#endif

/* End of combined C document. Each section can be split into separate .c files.
   If you want, I can split them into three separate files in the project folder and
   run compilation + tests here, or provide annotated explanations for each function.
*/
