# C-Based High-Performance AR Image Processing Acceleration  
## 基於 C 語言的 AR 影像處理與演算法加速

---

## 📌 專案簡介

本專案以 **C 語言實作 AR（Augmented Reality）影像處理核心演算法**，並比較：

1. **Baseline (原始版)**  
   - 傳統三層 for 迴圈  
   - 每次運算均檢查邊界  
   - 記憶體訪存未優化  

2. **Optimized (優化版)**  
   - 指標式運算  
   - 邊界與主運算區分處理  
   - 減少不必要分支與 memory access  
   - 指令與 Cache 更友善  

3. **OpenMP（可選）**  
   - 加入平行化  
   - CPU 多核心加速計算  

並對：

- 卷積（Convolution：模糊或銳化）
- Sobel 邊緣偵測（Edge Detection）

進行 **執行時間效能比較與可視化展示**。

---

## 🧠 為什麼和 AR 晶片有關？

AR 裝置（如 AR 眼鏡、手機 ISP、AI Camera）必須：

- 以極低延遲即時處理影像  
- 大量使用 Kernel Conv / Edge Detection  
- 在硬體中通常會設計  
  - ISP / Vision Accelerator  
  - CNN / MAC array  
  - 平行資料路徑  

本專案以 C 模擬 CPU 執行成本，再展示：

> 硬體若實作平行加速，可達 **數倍至數十倍效能提升與更低功耗**。

---

## 🗂 專案目錄結構

ar-hpc-project/
-│
-├── baseline.c # 原始三層迴圈實作
-├── optimized.c # 記憶體與運算優化版本
-├── openmp.c # OpenMP 平行版（可視平台啟用）
-│
-├── test_gray.bmp # 測試用灰階影像
-├── optimized_conv.bmp
-├── optimized_sobel.bmp
-│
-├── timings.csv # 各版本執行時間比較
-├── conv_times.png # 卷積效能圖
-├── sobel_times.png # Sobel 效能圖
-│
-└── README.md
---

## 🔧 編譯方式

### Baseline
```bash
gcc baseline.c -o baseline
Optimized
gcc optimized.c -o optimized
OpenMP（若需）
gcc openmp.c -o openmp -fopenmp
---

##▶ 執行方式
./baseline input.bmp output.bmp
./optimized input.bmp output.bmp
./openmp input.bmp output.bmp
⏱ 計時方法

為了跨平台（Windows / Linux / macOS）通用，本專案使用：

clock()


計算執行時間：

double elapsed_ms = 
    (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

📊 效能比較（示例）
方法	卷積時間(ms)	Sobel 時間(ms)	加速倍數
Baseline	120.3	258.1	1.0×
Optimized	47.8	112.4	2.5×
OpenMP (4 threads)	15.2	39.8	7.9×

詳細結果與圖：

timings.csv

conv_times.png

sobel_times.png

🧪 測試影像

專案內已提供：

test_gray.bmp（1024×768 隨機灰階 BMP）

程式會輸出：

optimized_conv.bmp

optimized_sobel.bmp

📷 效果展示
🔹 原始 vs Convolution

（可在投影片貼圖）

🔹 原始 vs Sobel Edge

（可在投影片貼圖）

🎯 專案收穫

實作 AR 常見影像運算

使用純 C 模擬硬體成本

展示高效能運算（HPC）思維

量化：

運算時間差異

訪存優化效果

多核心加速成果

📌 後續可擴充方向

SIMD（SSE / AVX / NEON）向量化

真實 ISP pipeline 模擬

CNN kernel 卷積加速

Cache / DRAM traffic 模型

FPGA / ASIC HLS 實作
