<div align="center">

# KAEDE PLAYER V2

**高精度發燒音訊播放器 · 原生硬體直通串流架構**

[![Latest Release](https://img.shields.io/github/v/release/KaedeNKD/Kaede-Player?style=for-the-badge&logo=github&logoColor=white&color=38B2CE)](https://github.com/KaedeNKD/Kaede-Player/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows_x64-22272E?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/KaedeNKD/Kaede-Player)
[![Graphics Acceleration](https://img.shields.io/badge/Graphics-全硬體_GPU_加速-41CD52?style=for-the-badge&logo=nvidia&logoColor=white)](https://github.com/KaedeNKD/Kaede-Player)
[![Vectorization](https://img.shields.io/badge/Vectorization-AVX2_%2B_FMA3-D97706?style=for-the-badge)](https://github.com/KaedeNKD/Kaede-Player)

<p align="center">
  專為追求純淨音質與極致重播精度所打造的 Windows 音訊播放器。<br/>
  融合硬體級位元完美直通、高頻動態聲學重構引擎與全硬體加速流暢介面。
</p>

</div>

---

> [!NOTE]
> **純淨音訊重播哲學**<br/>
> Kaede Player 採用原生硬體驅動直通與高精度 64 位元浮點運算架構，徹底繞過 Windows 系統混音器的染色與壓縮，還原錄音室母帶級的真實聲學動態。

---

### 核心功能與特色

<table>
  <tr>
    <td width="50%" valign="top">
      <b>01 · 原生硬體直通 (Bit-Perfect)</b>
      <br/><br/>
      • <b>ASIO 驅動原生直通</b>: 直接調用硬體驅動，支援最高 768 kHz PCM 與 Native DSD 點對點無損傳輸。<br/>
      • <b>實時硬體時脈審計</b>: 具備硬體時脈檢驗機制，確保 0 Hz 時脈偏差與絕對精確度。<br/>
      • <b>WASAPI 獨占管線</b>: 鎖定硬體音訊端點，徹底杜絕系統重採樣與音質劣質化。
    </td>
    <td width="50%" valign="top">
      <b>02 · 類神經頻譜空氣感重構 (Spectral BWE)</b>
      <br/><br/>
      • <b>高頻動態智慧修復</b>: 針對壓縮音軌及標準 CD 音源，智慧補齊被切除的超高頻泛音與空氣感。<br/>
      • <b>原生基頻完全直通</b>: 嚴格物理頻帶隔離技術，人聲與樂器基音 100% 保持原始位元純淨度。<br/>
      • <b>防過載防爆音保護</b>: 針對高度壓限的流行樂音軌主動優化動態，杜絕數位削波破音。
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <b>03 · 發燒級重採樣與 1-Bit DSD 調變</b>
      <br/><br/>
      • <b>超高階 Alien-FIR 濾波</b>: 具備線性相位與零前置振鈴等多種高保真重採樣模式。<br/>
      • <b>1-Bit 高階 DSD 調變器</b>: 將 PCM 音訊即時調變為細膩流暢的脈衝流輸出。<br/>
      • <b>聽覺感知噪聲整形</b>: 結合人耳聽覺特性優化動態背景，呈現深邃漆黑的聲音底色。
    </td>
    <td width="50%" valign="top">
      <b>04 · 全硬體加速與絲滑媒體庫</b>
      <br/><br/>
      • <b>0% CPU 圖形負擔</b>: 動態介面與聲學分析儀完全由 GPU 運算，不搶佔音樂播放資源。<br/>
      • <b>海量曲庫極速瀏覽</b>: 具備非同步封面加載管線，上萬首音軌滑動瀏覽穩定鎖定高更新率 (144Hz+)。<br/>
      • <b>高自由度標籤排序</b>: 支援自訂表達式與多維度標籤分類，輕鬆管理大型音樂庫。
    </td>
  </tr>
</table>

---

### 音訊處理架構

<div align="center">

<table width="100%">
  <tr>
    <td align="center">
      <b>音訊解碼輸入 (AUDIO INPUT)</b><br/>
      <code>FLAC</code> · <code>WAV</code> · <code>DSF / DFF (Native DSD)</code> · <code>MP3</code>
    </td>
  </tr>
  <tr>
    <td align="center">↓</td>
  </tr>
  <tr>
    <td>
      <b>01 · 類神經頻譜空氣感重構引擎 (NEURAL SPECTRAL BWE)</b><br/>
      <sub>自動鑑別音軌格式，修復高頻斷崖並拓展立體音場，同時保證原始人聲與基頻絕對純淨。</sub>
    </td>
  </tr>
  <tr>
    <td align="center">↓</td>
  </tr>
  <tr>
    <td>
      <b>02 · 數位主增益與耳機聲場空間化 (PRE-GAIN & CROSSFEED)</b><br/>
      <sub>立體聲耳機交叉饋送優化，消除長時間耳機聆聽的壓迫感與頭中效應。</sub>
    </td>
  </tr>
  <tr>
    <td align="center">↓</td>
  </tr>
  <tr>
    <td>
      <b>03 · 高精度參數化等化器 (PARAMETRIC EQ)</b><br/>
      <sub>高精確度即時濾波器矩陣，提供精準細膩的頻率響應校正。</sub>
    </td>
  </tr>
  <tr>
    <td align="center">↓</td>
  </tr>
  <tr>
    <td>
      <b>04 · 重採樣與高階調變後端 (RESAMPLING & MODULATION)</b><br/>
      <sub>發燒級多相 Sinc 插值濾波與高階 1-Bit 脈衝調變輸出。</sub>
    </td>
  </tr>
  <tr>
    <td align="center">↓</td>
  </tr>
  <tr>
    <td align="center">
      <b>硬體串流輸出 (HARDWARE OUTPUT)</b><br/>
      <code>Direct In-Proc ASIO (0 Hz 時脈偏差)</code> · <code>WASAPI 獨占模式</code>
    </td>
  </tr>
</table>

</div>

---

### 系統支援與規格

<details>
<summary><b>點擊查看系統環境需求與音訊支援</b></summary>
<br/>

| 項目類別 | 規格說明 |
| :--- | :--- |
| **作業系統** | Windows 10 / 11 (64-bit) |
| **處理器需求** | 支援 AVX2 與 FMA3 向量指令集之現代 64 位元處理器 |
| **圖形加速** | 全硬體 GPU 加速渲染 (Direct3D 11, Direct3D 12, Vulkan) |
| **音訊支援** | ASIO 2.0+ 原生設備、WASAPI 獨占支援之各類外接 DAC 與音效卡 |
| **音訊格式** | DSD (DSF, DFF), FLAC, WAV, MP3 等主流無損與壓縮格式 |

</details>

<details>
<summary><b>點擊查看多語言支援</b></summary>
<br/>

| 語言 | 支援範圍 |
| :--- | :--- |
| **正體中文 (繁體)** | 完整支援 (核心介面 · 聲學儀表 · 說明指南) |
| **简体中文** | 完整支援 (核心界面 · 声学仪表 · 说明指南) |
| **日本語** | 完整支援 (コアUI · アナライザー · 技術ガイド) |
| **English** | 完整支援 (Native UI) |

</details>

---

<div align="center">
<sub>Designed and Developed by KaedeNKD · High-Precision Audio Engineering</sub>
</div>
