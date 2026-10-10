# STM32 控制板硬體設計分析與 ST-LINK/V2 燒錄接線確認

> 分析日期：2026-10-09
> 分析方式：靜態檔案分析（僅讀取，未做任何實體量測或燒錄）
> 重要聲明：本報告所有結論均標註證據來源；無法從現有可讀檔案證實的連接，一律標示為「未確認」。

---

## 0. 找到的設計文件（證據盤點）

在 `C:\Users\yuchiao\Desktop\THUR\` 下找到兩套 Altium Designer 硬體設計：

| 資料夾 | 內容 | 可讀性 |
|--------|------|--------|
| `老数采数传板子`（老數據採集傳輸板） | 多頁 SchDoc + 多版 PcbDoc + `AEBS_controller_STM32_00.pdf` | SchDoc/PcbDoc 為 Altium 二進位，PDF 本模型無法讀取 |
| `1-AEBS_STM32_485`（AEBS_STM32_485 板） | 同上 + `AEBS_485_Relay_00_BOM表.csv` + `Pick Place for AEBS_485_00.txt` + `AEBS_485_00.pdf` | 同上，但 BOM CSV 與 Pick Place TXT 為純文字，**可讀** |

**缺少、無法取得關鍵連接資訊的文件**：
- Altium `.SchDoc` / `.PcbDoc` 為專有二進位格式，無法在此環境解析出完整的 pin-to-net 連接表。
- `.pdf` 原理圖（`AEBS_485_00.pdf`、`AEBS_controller_STM32_00.pdf`）本模型不支援 PDF 輸入，無法讀取圖面。

因此，**P5/P6 的完整針腳對照表無法從現有可讀資料百分之百重建**。以下「已確認」項目來自 BOM CSV、Pick Place TXT 以及從 SchDoc 二進位檔中提取出的文字片段（元件編號、引腳名、NetLabel 名稱）。

---

## 1. 控制板硬體架構及晶片型號

### 1.1 晶片：STM32F105VCT6（已確認）

| 項目 | 值 | 證據 |
|------|-----|------|
| 型號 | **STM32F105VCT6** | BOM CSV：`SigModule_00.SCHLIB\STM32F105VCT6`（Footprint=LQFP100）；SchDoc 文字：`LibReference=STM32F105VCT6, AllPinCount=100` |
| 封裝 | LQFP100 | 同上 |
| 核心 | Cortex-M3（Connectivity Line） | ST 官方規格 |
| Flash / SRAM | 256 KB / 64 KB | STM32F105VC 標準規格 |
| 工作電壓 | 2.0–3.6 V（本板採 3.3 V） | 規格 + 本板 VCC33 NetLabel |
| 除錯介面 | **SWD + JTAG 均支援** | STM32F105 內建 |
| 韌體起始位址 | Flash 0x08000000 | STM32 標準 |

### 1.2 SWD/JTAG 相關 MCU 引腳（已確認，取自 SchDoc 引腳名）

| MCU 引腳號 | 引腳名 | 功能 |
|-----------|--------|------|
| 72 | PA13 / JTMS-SWDIO | **SWDIO**（SWD 資料線） |
| 76 | PA14 / JTCK-SWCLK | **SWCLK**（SWD 時脈） |
| 77 | PA15 / JTDI | JTAG TDI |
| 89 | PB3 / JTDO | JTAG TDO |
| 90 | PB4 / NJRST | JTAG nTRST |
| 14 | NRST | 系統復位 |
| 94 | BOOT0 | 啟動模式選擇 |
| 37 | PB2 / BOOT1 | 啟動模式選擇 |

### 1.3 電源架構（已確認）

```
12 V 輸入 → TLE8366EV50 → 5 V → AS1117-3.3V → 3.3 V（VCC33）
```

證據：BOM CSV 含 `TLE8366EV50`、`AS1117-3.3V`；SchDoc 含 `VCC33`、`VCC5` NetLabel。

---

## 2. P6 / P5 接口定位（關鍵發現）

### 2.1 Pick Place（已確認，文字檔直接讀取）

`1-AEBS_STM32_485\...\Pick Place for AEBS_485_00.txt` 明確列出：

| Designator | Comment | Footprint | 說明 |
|-----------|---------|-----------|------|
| **P5** | **JTAG20** | **JTAG_20** | "IDC-20P(10x2, 2.54mm)" ← **20-pin 2×10 接口是 P5** |
| P6 | USB_MINI_AB | USB-MINI-A | USB Mini AB 插座（**非 20-pin**） |
| P4 | BT04-E | SIP6 | 藍牙模組（6-pin，非 20-pin） |
| P2 | DB9-F | DB9SLE_FEMALE | DB9 母座 |
| P1 | AMP-175975-2 | AMP-175975-2 | Header 10-pin 雙排 |

### 2.2 SchDoc（已確認）

`AEBS_controller_STM32_00.SchDoc` / `STM32_01.SchDoc`（兩套板一致）：

- `P5` = `LibReference=HEADER 10X2`，`Comment=JTAG20`，`ComponentDescription=IDC-20P(10x2, 2.54mm)`，`AllPinCount=20`。

### 2.3 結論

> **設計文件中的 20-pin 2×10 燒錄接口編號是「P5」，不是「P6」。**
> **P6 在 485 板是 USB Mini AB 插座；在老板則出現在 DB9 連接器頁與 XC2000 頁。**

這與使用者描述的「P6 為 20-pin」**直接矛盾**。可能原因：(a) 絲印編號誤記；(b) 手上實物與這兩套設計檔不同。**在確認之前，不建議任何接線動作。**

---

## 3. P5（20-pin 接口）所接信號（部分已確認）

從 SchDoc 二進位提取的 NetLabel 名稱，出現在 P5 元件（JTAG20）附近的僅有下列 4 個：

| 信號 | 證據（SchDoc NetLabel 文字） |
|------|------------------------------|
| **VCC33** | `TEXT=VCC33`（出現在 P5 引腳區） |
| **SWDIO** | `TEXT=SWDIO`（位置對應 P5 引腳區） |
| **SWCLK** | `TEXT=SWCLK`（位置對應 P5 引腳區） |
| **GND** | `TEXT=GND`（出現在 P5 引腳區） |

且 MCU 端確認：`SWDIO` NetLabel → `PA13/JTMS-SWDIO`（pin 72）；`SWCLK` NetLabel → `PA14/JTCK-SWCLK`（pin 76）。

### 3.1 重要推論

> P5 這個 20-pin 接口，原理圖上**只接出 4 個信號（VCC33 / SWDIO / SWCLK / GND）**，
> 是一個「簡化的 SWD 接口」，**不是**完整 JTAG 的標準 20-pin 佈局
> （缺 TDI/TDO/TCK/TMS/nTRST/nRST 等標準 JTAG 信號的接出證據）。

### 3.2 P5 完整 20-pin 對照表（由原理圖座標重建）

> 方法：從 `AEBS_controller_STM32_00/01.SchDoc` 二進位中提取 P5（HEADER 10X2）之
> 20 個引腳座標，以及引腳區的 NetLabel 與 Wire 端點座標，依 **Y 座標一一對應**重建。
> 重建結果與 NetLabel 座標完全吻合（VCC33↔Pin1@Y=255、SWDIO↔Pin7@Y=225、SWCLK↔Pin9@Y=215、
> 右排偶數腳經豎線接 GND）。**此為座標重建，非直接讀出的 netlist，燒錄前仍建議萬用表複核。**

| P5 Pin | 訊號 | 說明 |
|--------|------|------|
| 1 | **VCC33** | 3.3 V（供 ST-LINK VTref 電壓參考，不供電） |
| 2 | **GND** | |
| 3 | NC | 未接 |
| 4 | **GND** | |
| 5 | NC | 未接 |
| 6 | **GND** | |
| 7 | **SWDIO** | ← PA13/JTMS-SWDIO |
| 8 | **GND** | |
| 9 | **SWCLK** | ← PA14/JTCK-SWCLK |
| 10 | **GND** | |
| 11 | NC | 未接 |
| 12 | **GND** | |
| 13 | NC | 未接 |
| 14 | **GND** | |
| 15 | NC | 未接（**無 nRST 接出**） |
| 16 | **GND** | |
| 17 | NC | 未接 |
| 18 | **GND** | |
| 19 | NC | 未接 |
| 20 | **GND** | |

> 規律：**Pin 1 = VCC33、Pin 7 = SWDIO、Pin 9 = SWCLK、所有偶數腳 = GND、其餘奇數腳 = NC**。
> 注意：**P5 未接出 NRST**，ST-LINK 無法經此接口對 MCU 做硬體復位（SWD 仍可燒錄，靠軟體復位/重新上電）。

---

## 4. ST-LINK/V2 與 P5 的接線對照表

ST 官方 ST-LINK/V2 的 20-pin（CN2）為標準 ARM JTAG 20-pin 佈局。與本板 P5 逐腳比對如下：

| ST-LINK/V2 20-pin | 訊號 | 本板 P5 | 訊號 | 相容性 |
|-------------------|------|---------|------|--------|
| 1 | VTref | 1 | VCC33 | ✅ 匹配（3.3V 參考） |
| 2 | TDI / NC | 2 | GND | ⚠️ 見下方說明 |
| 3 | nTRST | 3 | NC | ✅ 無害（ST-LINK 輸出接懸空） |
| 4 | GND | 4 | GND | ✅ 匹配 |
| 5 | TDI | 5 | NC | ✅ 無害 |
| 6 | GND | 6 | GND | ✅ 匹配 |
| 7 | **TMS/SWDIO** | 7 | **SWDIO** | ✅ 匹配 |
| 8 | GND | 8 | GND | ✅ 匹配 |
| 9 | **TCK/SWCLK** | 9 | **SWCLK** | ✅ 匹配 |
| 10 | GND | 10 | GND | ✅ 匹配 |
| 11 | RTCK/NC | 11 | NC | ✅ 匹配 |
| 12 | GND | 12 | GND | ✅ 匹配 |
| 13 | TDO | 13 | NC | ✅ 無害 |
| 14 | GND | 14 | GND | ✅ 匹配 |
| 15 | nRST | 15 | NC | ✅ 無害（但無硬體復位） |
| 16 | GND | 16 | GND | ✅ 匹配 |
| 17 | NC | 17 | NC | ✅ 匹配 |
| 18 | GND | 18 | GND | ✅ 匹配 |
| 19 | NC | 19 | NC | ✅ 匹配 |
| 20 | GND | 20 | GND | ✅ 匹配 |

**關於 Pin 2 的唯一爭議點**：ST-LINK/V2 的 Pin 2 依版本/克隆可能是 **TDI**（JTAG 訊號）或 **NC**。
本板 Pin 2 = **GND**。若 ST-LINK 在 **SWD 模式**下操作，TDI 為高阻三態，接到 GND 無害；
但若誤入 **JTAG 模式**或該克隆 ST-LINK 會驅動 TDI，則 Pin 2 有短路風險。

**結論：只要用 SWD 模式，20-pin 排線在電氣上可直插（除 Pin 2 需注意 TDI 三態）。**

---

## 5. 排線能否直接連接（明確結論）

### 回答：**可以直插，但必須用 SWD 模式，且連接前先用萬用表複核 VCC/GND 位置。**

1. **編號已澄清**：20-pin 接口確認是 **P5**（JTAG20/HEADER 10X2），P6 為 USB。
2. **SWD 核心腳位完全吻合**：本板 P5 的 Pin1=VCC33、Pin7=SWDIO、Pin9=SWCLK、偶數腳=GND，
   與 ST-LINK/V2 20-pin 的 VTref/TMS-SWDIO/TCK-SWCLK/GND 位置**一一對應**（見第 4 節）。
3. **必須 SWD 模式**：板無 JTAG 的 TDI/TDO/nTRST 接出，勿用 JTAG 模式。
4. **唯一風險 Pin 2**：ST-LINK 之 TDI 可能接到本板 GND，SWD 模式下 TDI 三態通常無害，
   但為保險，可將排線 Pin 2 那條線剪斷/退針，或直接改用杜邦線只接 4 線。

### Pin 1 方向（紅色邊朝哪邊）

- 原理圖上 P5 的 Pin 1 位於元件**左上角**（座標 X=215、Y=255 為最上方一列）。
- **實物方向**：PCB 上 P5 的 JTAG_20 footprint 有 90° 旋轉（Pick Place 記載 Rotation=90），
  且通常有「▲/1」絲印或方形焊盤標記 Pin 1。
- **安全做法**：上電前以萬用表量測——**排線 Pin 1 應為 VCC33（≈3.3V），偶數腳對 GND 應導通**，
  確認後再對齊排線紅色邊（Pin 1）插入。

### 若使用杜邦線（最穩妥，只接 4 線）

| ST-LINK/V2 | P5 |
|-----------|-----|
| SWDIO（或 20-pin Pin7） | Pin 7 |
| SWCLK（或 20-pin Pin9） | Pin 9 |
| GND（任一 GND） | Pin 2 / 4 / 6…（任偶數腳） |
| VTref（20-pin Pin1，僅參考，不供電） | Pin 1 |

---

## 6. 供電與接線風險

| 項目 | 狀態 / 結論 |
|------|-------------|
| 控制板供電 | 板內有完整電源鏈（12V→5V→3.3V），**應以板上 12V 獨立供電** |
| ST-LINK 供電 | **不得**由 ST-LINK 向板供電；其 VTref/VCC 腳僅作邏輯電壓參考 |
| 電壓匹配 | 目標 VCC33 = 3.3V；ST-LINK/V2 支援 1.65–3.6V 參考，SWD 邏輯電平應匹配 3.3V |
| 反向供電風險 | 若 20-pin 直連且針腳定義不符，VCC 腳可能落到非電源腳，存在損壞風險（**未確認前禁止直連**） |
| GND 共地 | 燒錄前必須確保 ST-LINK GND 與板 GND 共地 |

---

## 7. Ubuntu 燒錄操作指南（僅在接線確認後執行）

> 以下為 STM32CubeProgrammer 操作，**前提是已完成第 5 節的接線確認**。
> 在確認備份與正確韌體前，**不得執行 Mass Erase、解除讀取保護或任何寫入**。

```bash
# 1. 安裝（若未裝）
#    STM32CubeProgrammer CLI 位於安裝目錄，通常：
#    /usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI

STM32_Programmer_CLI=STM32_Programmer_CLI   # 或完整路徑

# 2. 識別 ST-LINK 裝置
$STM32_Programmer_CLI --list

# 3. 連接並讀取晶片資訊（Device ID / 讀取保護狀態）
$STM32_Programmer_CLI -c port=SWD mode=UR freq=4000

# 4. 讀取並備份現有 Flash（若讀取保護未啟用；ST 內部 Bootloader 非本報告範圍）
$STM32_Programmer_CLI -c port=SWD mode=UR -r 0x08000000 0x40000 backup.bin

# 5. 確認韌體檔案與燒錄位址
#    韌體為 .hex 或 .bin；起始位址 0x08000000（STM32F105 Flash）

# 6. 寫入並驗證
$STM32_Programmer_CLI -c port=SWD mode=UR -w firmware.hex -v

# 7. 復位與啟動
$STM32_Programmer_CLI -c port=SWD mode=UR -rst
```

---

## 8. 尚未確認的資訊與下一步檢查方法

| 待確認 | 檢查方法 |
|--------|----------|
| P5 實物 Pin 1 方位（排線紅色邊朝向） | 目視 PCB 絲印「▲/1」標記；或萬用表量 VCC33(≈3.3V) 與 GND 定位 |
| ST-LINK/V2 之 Pin 2 是否為 TDI 且有無驅動 | 查該 ST-LINK 型號文件；或直接用杜邦線避開 Pin 2 |
| pinout 重建是否 100% 正確（座標重建，非 netlist） | 用 Altium 開啟 SchDoc 檢視 P5 頁；或萬用表量每腳對 GND/VCC |

**安全順序（建議）**：
1. 上電，用萬用表量 P5：找 VCC33（≈3.3V）與 GND，確認 Pin 1 / 偶數腳位置與重建表一致。
2. 用杜邦線接 SWDIO/SWCLK/GND 三線（或直接插排線但先確認 Pin1 方向）。
3. STM32CubeProgrammer 以 **SWD** 模式連接，讀 Device ID（應為 STM32F105，Device ID=0x418）。
4. 讀取成功後，備份現有 Flash，再寫入韌體。

---

## 附：本報告可回溯的證據檔案清單

- `C:\Users\yuchiao\Desktop\THUR\1-AEBS_STM32_485\Project Outputs for AEBS_485_Relay_00\AEBS_485_Relay_00_BOM表.csv`（晶片型號、電源 IC、連接器 footprint）
- `C:\Users\yuchiao\Desktop\THUR\1-AEBS_STM32_485\Project Outputs for AEBS_485_Relay_00\Pick Place for AEBS_485_00.txt`（P5=JTAG20、P6=USB 等 Designator 對照）
- `C:\Users\yuchiao\Desktop\THUR\1-AEBS_STM32_485\AEBS_controller_STM32_01.SchDoc`（二進位字串提取：MCU 引腳名、SWDIO/SWCLK/VCC33/GND NetLabel、P5 元件定義）
- `C:\Users\yuchiao\Desktop\THUR\老数采数传板子\AEBS_controller_STM32_00.SchDoc`（同結構，兩套板一致）
