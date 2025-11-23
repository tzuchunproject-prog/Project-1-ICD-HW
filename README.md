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

