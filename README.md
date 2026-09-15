<div align="center">

# KAEDE PLAYER V2

**高精度本機音訊重播引擎 · 原生硬體直通與多核 DSP 管線架構**

[![Latest Release](https://img.shields.io/github/v/release/KaedeNKD/Kaede-Player?style=for-the-badge&logo=github&logoColor=white&color=38B2CE)](https://github.com/KaedeNKD/Kaede-Player/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows_x64-22272E?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/KaedeNKD/Kaede-Player)
[![Graphics Acceleration](https://img.shields.io/badge/Rendering-Qt_RHI_(D3D11%2F12%20%2F%20Vulkan)-41CD52?style=for-the-badge&logo=vulkan&logoColor=white)](https://github.com/KaedeNKD/Kaede-Player)
[![Vectorization](https://img.shields.io/badge/Vectorization-AVX2_%2B_FMA3-D97706?style=for-the-badge)](https://github.com/KaedeNKD/Kaede-Player)
[![Afdian Sponsor](https://img.shields.io/badge/Sponsor-愛發電-946ce6?style=for-the-badge&logo=kofi&logoColor=white)](https://afdian.com/a/kaedeNKD?utm_source=copylink&utm_medium=link)

<p align="center">
  專為極限重播精度打造的現代 Windows 音訊播放器。<br/>
  結合底層硬體直通管線、多演算法高階 FIR 重採樣陣列與零 CPU 佔用 GPU 圖形遙測。
</p>

</div>

---

> [!NOTE]
> **架構設計原則**<br/>
> Kaede Player 全鏈路採用 64-bit 雙精度浮點運算與進程內原生驅動直通，徹底繞過 Windows Audio Session API 共享混音器與系統 SRC（採樣率轉換），保證音訊串流在時間域與頻率域的位元完整性（Bit-Perfect）。

---

### 核心系統架構與特性

<table>
  <tr>
    <td width="50%" valign="top">
      <b>01 · 原生硬體直通管線 (Bit-Perfect Pipeline)</b>
      <br/><br/>
      • <b>ASIO 2.0+ 原生直調用</b>: 直接與音訊硬體驅動溝通，支援最高 768 kHz PCM 與 Native DSD 點對點傳輸。<br/>
      • <b>實時硬體時脈審計 (Clock Audit)</b>: 內建硬體時脈漂移監控機制，確保 0 Hz 時脈偏差與時間戳精確對齊。<br/>
      • <b>WASAPI 獨占端點 (Exclusive Mode)</b>: 鎖定底層音訊端點，消除系統層混音染色與緩衝區抖動。
    </td>
    <td width="50%" valign="top">
      <b>02 · 頻譜頻寬擴展與動態防護 (Spectral BWE)</b>
      <br/><br/>
      • <b>頻譜頻寬擴展 (BWE)</b>: 針對受限音軌重建高頻諧波與能量分佈，補齊奈奎斯特頻率截斷邊界。<br/>
      • <b>物理頻帶隔離</b>: 基頻與人聲主頻帶 100% 直通處理，杜絕相位調製失真。<br/>
      • <b>True Peak 浮點防削波</b>: 64 位元浮點 Headroom 管理搭配低失真軟限幅，防止高電平音軌數位溢位破音。
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <b>03 · 多核 FIR 重採樣與高階調變 (Resampling & SDM)</b>
      <br/><br/>
      • <b>四大數學濾波核</b>:
        <br/>&emsp;– <b>SDP-FIR</b>: 凸優化線性相位，極致帶外衰減。
        <br/>&emsp;– <b>Zenith DPSS</b>: Slepian 離散長橢球序列最優窗，動態分配最高 131,072 Taps。
        <br/>&emsp;– <b>APK</b>: 非對稱時域遮蔽，消除人耳敏感的前置振鈴（Pre-ringing）。
        <br/>&emsp;– <b>CAMFIR</b>: 內容自適應動態混合，支援即時 Morph Ratio 連續形變。
      <br/>
      • <b>AVX2 / FMA3 向量加速</b>: 多相濾波（Polyphase）核心指令集優化，維持極低延遲。<br/>
      • <b>高階 Sigma-Delta (SDM) 調變</b>: 整合聽覺加權噪聲整形（Noise Shaping），將量化噪聲推移至可聽頻段之外。
    </td>
    <td width="50%" valign="top">
      <b>04 · 零 CPU 繪圖開銷與現代媒體庫 (RHI Engine)</b>
      <br/><br/>
      • <b>Qt RHI 硬體渲染</b>: 圖形後端支援 Direct3D 11/12 與 Vulkan，頻譜分析與 UI 運算 100% 卸載至 GPU。<br/>
      • <b>無鎖非同步遙測 (Lock-Free Telemetry)</b>: 即時聲學 HUD 透過環形緩衝區與音訊執行緒解耦，完全不佔用 DSP 算力。<br/>
      • <b>虛擬化媒體庫引擎</b>: 支援非同步封面快取與多維度元數據過濾，萬首曲庫捲動穩定鎖定高更新率 (144Hz+)。
    </td>
  </tr>
</table>

---
### 系統規格與執行環境

<details>
<summary><b>硬體與環境需求</b></summary>
<br/>

| 項目類別 | 規格要求 |
| :--- | :--- |
| **作業系統** | Windows 10 / Windows 11 (64-bit) |
| **處理器架構** | x86-64 現代架構（需完整支援 AVX2 與 FMA3 向量指令集） |
| **圖形硬體** | 支援 Direct3D 11 / Direct3D 12 或 Vulkan 1.2+ 之 GPU |
| **音訊硬體** | 支援 ASIO 2.0+ 或 WASAPI 獨占模式的外接 USB DAC、PCIe 音效卡 |
| **支援格式** | PCM (FLAC, WAV, MP3 等，最高 768 kHz / 32-bit)、DSD (DSF, DFF, 支援 Native DSD) |

</details>

<details>
<summary><b>多語言支援 (Localization)</b></summary>
<br/>

| 語言代碼 | 語言名稱 | 涵蓋範圍 |
| :--- | :--- | :--- |
| `zh_TW` | 正體中文 | 完整介面 · DSP 矩陣控制面板 · 遙測 HUD · 技術說明 |
| `zh_CN` | 简体中文 | 完整界面 · DSP 矩阵控制面板 · 遥测 HUD · 技术说明 |
| `ja_JP` | 日本語 | フルUI · DSPマトリクス設定 · テレメトリHUD · 技術仕様 |
| `en_US` | English | Full UI · DSP Matrix Panel · Telemetry HUD · Specs |

</details>

---

### 支持與贊助 (Sponsorship)

Kaede Player 是一項獨立架構的非商業/開源音訊工程專案。如果你認同本專案對極致重播精度與 DSP 演算法的探索，歡迎透過愛發電支持後續的演算法研發、算力維護與功能反覆運算：

[![Afdian](https://img.shields.io/badge/Afdian-贊助作者支持開發-946ce6?style=for-the-badge&logo=kofi&logoColor=white)](https://afdian.com/a/kaedeNKD?utm_source=copylink&utm_medium=link)

*特別贊助者將於後續版本與 V3 商業化進程中，永久銘刻於專案關於視窗與鳴謝名單。*

---

<div align="center">
<sub>Designed and Developed by KaedeNKD · Audio DSP & Systems Architecture</sub>
</div>
