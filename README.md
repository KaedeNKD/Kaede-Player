<div align="center">

# KAEDE PLAYER V2

**高精度本機音訊重播架構 · 原生硬體直通與 212-bit 非 LTI 數值管線**

[![Version](https://img.shields.io/badge/Version-v2.9.5-38B2CE?style=for-the-badge)](https://github.com/KaedeNKD/Kaede-Player/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows_x64-22272E?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/KaedeNKD/Kaede-Player)
[![Graphics Backend](https://img.shields.io/badge/Graphics-Qt_RHI_(D3D11%2F12%20%7C%20Vulkan)-41CD52?style=for-the-badge&logo=vulkan&logoColor=white)](https://github.com/KaedeNKD/Kaede-Player)
[![SIMD Acceleration](https://img.shields.io/badge/Instruction-AVX2_%2B_FMA3-D97706?style=for-the-badge)](https://github.com/KaedeNKD/Kaede-Player)
[![Sponsorship](https://img.shields.io/badge/Sponsor-Afdian-946ce6?style=for-the-badge)](https://afdian.com/a/kaedeNKD?utm_source=copylink&utm_medium=link)

<p align="center">
  專為極限重播精度打造的 Windows 音訊播放環境。<br/>
  結合位元完美硬體直通、212-bit 超雙精度多相濾波陣列、非 LTI 連續力學動態與零 CPU 繪圖開銷遙測。
</p>

</div>

---

### 設計準則

Kaede Player 採用進程內驅動直通與全鏈路高精度運算架構。系統徹底繞過 Windows Audio Session (WASAPI) 共享混音器、系統級 SRC（Sample Rate Converter）與作業系統音量衰減器，杜絕非整數倍重採樣引發的互調失真與時間域相位扭曲，確保 DAC 硬體接收到位元完整（Bit-Perfect）的音訊資料。

---

### 核心子系統

<table>
  <tr>
    <td width="50%" valign="top">
      <b>01 · 原生硬體直通與虛擬橋接</b>
      <br/><br/>
      • <b>ASIO 2.0+ / WASAPI 獨占直通</b>：跳過作業系統音訊引擎，支援最高 768 kHz PCM 與 Native DSD1024 點對點無損輸出。<br/>
      • <b>硬體時脈審計與單調對齊</b>：高精度 QPC 硬體計時器與 DAC 實體發聲時鐘嚴密同步，杜絕播放時間漂移與取樣累積誤差。<br/>
      • <b>Kaede Virtual ASIO 影子橋接</b>：內建進程間低延遲共享記憶體（IPC），允許外部 DAW（Cubase, REAPER 等）將音訊串流直接注入播放器 DSP 管線。
    </td>
    <td width="50%" valign="top">
      <b>02 · 212-bit 極限精度與非 LTI 重採樣</b>
      <br/><br/>
      • <b>212-bit 極限數值精度</b>：核心重採樣卷積與連續微積分全域運行於 212-bit 超雙精度管線，消除長鏈路運算累積的中繼捨入截斷雜訊。<br/>
      • <b>破除 LTI 靜態假設</b>：揚棄傳統音訊 DSP 依賴線性時不變（LTI）系統的僵化框架，引入連續力學動態，針對瞬態衝擊進行微觀自適應演化，兼具衝激定位與零時域模糊。<br/>
      • <b>多相 FIR 與高階 SDM 調變</b>：最高 131,072 Taps，帶外阻帶衰減低於 -180 dBFS；內建最高 15 階閉環調變器，支援將 PCM 即時升頻至 DSD1024。
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <b>03 · 聲學防護與空間拓撲矩陣</b>
      <br/><br/>
      • <b>True Peak 浮點防削波防線</b>：64-bit 浮點 Headroom 管理搭配高動態餘量軟限幅，徹底防止高電平音軌或重採樣產生的採樣間峰值破音。<br/>
      • <b>頻譜頻寬擴展 (BWE)</b>：基頻與人聲主頻帶維持 100% 物理直通，僅對截斷邊界進行物理頻帶隔離的高頻諧波能量重構。<br/>
      • <b>ITU-R BS.775 多聲道等功率下混</b>：多聲道影片與音訊支援 64-bit FMA 矩陣下混為純淨立體聲，或透過次世代演算法上混至多聲道配置。
    </td>
    <td width="50%" valign="top">
      <b>04 · D3D11VA 全景影院與跨端遙控</b>
      <br/><br/>
      • <b>KCM 全景影院艙</b>：整合 D3D11VA 硬體解碼，支援 4K 10-bit HDR/BT.2020 影片播放，具備 ASS/SSA 雙語字幕智慧切分與高解析度點陣排版。<br/>
      • <b>Qt RHI 零 CPU 繪圖開銷</b>：UI 與即時頻譜/相位雷達 100% 卸載至 GPU（D3D11/12 或 Vulkan），保證百萬首曲庫以 144Hz+ 流暢捲動。<br/>
      • <b>局域網 Web Remote</b>：內建 HTTP/WebSocket 遙控伺服器，支援行動端瀏覽器 60FPS 本地時脈平滑外推、手勢切換與存取權限控制。
    </td>
  </tr>
</table>

---

### 技術規格與運算精度

| 項目模組 | 運算精度 / 實現規範 | 效能與硬體指標 |
| :--- | :--- | :--- |
| **運算數值精度** | 212-bit Quad-Double / 64-bit 雙精度浮點數 | 理論數值底噪低於 -600 dBFS 等效動態範圍 |
| **系統建模範式** | 突破傳統 LTI 假定之連續力學動態 | 消除傳統高階濾波器的時域擴散與衝激模糊 |
| **向量指令集加速** | AVX2 + FMA3 (256-bit SIMD 暫存器常駐) | 運算密集區段無棧記憶體往返，確保即時重播零掉幀 |
| **多相重採樣 (FIR)** | 多精度多相卷積 / 最高 131,072 Taps | 阻帶衰減: < -180 dBFS · 4 種相位響應模式可選 |
| **Sigma-Delta 調變** | 5 階 / 8 階 / 15 階 CIFF 閉環調變結構 | 支援 DSD64 至 DSD1024 (2.82 MHz ~ 45.15 MHz) 即時輸出 |
| **輸出傳輸模式** | ASIO 2.0+ (Native DSD / DoP) / WASAPI 獨占直通 | 位元完美直通，音訊時鐘與 QPC 硬體同步 |
| **虛擬音效卡橋接** | Kaede Virtual ASIO (進程間共享記憶體 IPC) | 核心傳輸延遲: < 1.2 ms，支援外部 DAW 串流直灌 |
| **視訊硬體加速** | Direct3D 11 Video Acceleration (D3D11VA) | 支援 4K H.264 / HEVC 10-bit P010 及多軌字幕智慧切分 |
| **圖形遙測管線** | Qt Rendering Hardware Interface (RHI) | D3D11 / D3D12 / Vulkan 直調用，0% CPU 音訊執行緒佔用 |

---

### 系統環境要求

* **作業系統**：Windows 10 / Windows 11 (64-bit)
* **處理器**：x86-64 架構處理器，**必須**完整支援 AVX2 與 FMA3 指令集（Intel 4 代 Core / AMD Zen 1 及更高階架構）
* **圖形處理器**：支援 Direct3D 11、Direct3D 12 或 Vulkan 1.2+ 之顯示卡
* **音訊介面**：支援 ASIO 2.0+ 驅動或 WASAPI Exclusive 獨占模式的外接 USB DAC、PCIe 專業音效卡
* **支援格式**：
  * **音訊**：FLAC, WAV, MP3, M4A, AAC, DSF, DFF（支援 Native DSD 1024 Max / DoP DSD128 - 256 Max）
  * **視訊**：MKV, MP4, MOV, WEBM, AVI（支援 ASS, SSA, SRT, VTT 內嵌與外掛字幕）

---

### 多語言支援 (Localization)

| 語言代碼 | 語言名稱 | 涵蓋範圍 |
| :--- | :--- | :--- |
| `zh_TW` | 繁體中文 | 完整介面 · DSP 矩陣控制面板 · 遙測 HUD · 影院控制台 |
| `zh_CN` | 简体中文 | 完整界面 · DSP 矩阵控制面板 · 遥测 HUD · 影院控制台 |
| `ja_JP` | 日本語 | フルUI · DSPマトリクス設定 · テレメトリHUD · シネマコンソール |
| `en_US` | English | Complete UI · DSP Matrix Panel · Telemetry HUD · Cinema Console |

---

### 贊助與支援 (Sponsorship)

Kaede Player 是一項聚焦於極限重播精度與音訊演算法實踐的獨立工程專案。如果你認同本專案對位元完美硬體直通、212-bit 數值管線與零 CPU 繪圖開銷的工程實踐，歡迎透過愛發電支持後續的演算法優化與維護：

[![Afdian](https://img.shields.io/badge/Afdian-贊助作者支持開發-946ce6?style=for-the-badge)](https://afdian.com/a/kaedeNKD?utm_source=copylink&utm_medium=link)

---

<div align="center">
<sub>Designed and Developed by KaedeNKD · Audio DSP & Systems Architecture</sub>
</div>
