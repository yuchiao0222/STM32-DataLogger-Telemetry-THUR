# AEBS_RF_Test 板級引腳映射（BOARD PINMAP）

> 本文件依據工程內 `.ioc`、`Core/Inc/*.h`、`Core/Src/*.c` 實際內容整理。
> ADC / CAN 之板級實際走線由硬體確認提供（見第 4、5 節）。
> 注意：`D:\THUR\LV_Power_Distribute_THUR_2025.eprj2` 為另一塊完全無關的板，
> 禁止以它判斷本 AEBS_RF_Test 之硬體。

## 0. 概覽

| 項目 | 內容 |
|------|------|
| MCU | STM32F105VCT6（Connectivity Line，Cortex-M3，LQFP100） |
| Flash / SRAM | 256 KB / 64 KB |
| 系統時脈 | 72 MHz（HSE 8 MHz × PLL9） |
| 工具鏈 | Keil MDK-ARM V5.32（ARMCLANG V6.24，AC6） |
| CubeMX 外設 IP（現況） | NVIC、RCC、SYS、USART1（共 4 項） |
| 已啟用 HAL 模組（現況） | GPIO、UART、CORTEX、DMA、FLASH、EXTI、PWR、RCC |

## 1. 狀態圖例

| 標記 | 狀態 | 判定標準 |
|------|------|----------|
| 【已實作】 | 已實作 | 有 HAL 初始化，且執行期程式碼確實讀寫該腳位/外設 |
| 【已配置未使用】 | 已配置但未使用 | 有 `.ioc` 配置與 HAL 初始化，但執行期無對應程式碼 |
| 【板級已確認】 | 板級已確認、尚未配置 | 硬體走線已確認，但 `.ioc` 與程式碼尚未實作 |
| 【尚未實作】 | 尚未實作 | `.ioc` 無此 IP，無初始化程式碼，無驅動檔案 |

---

## 2. GPIO 引腳映射

### 2.1 已實作

| 引腳 | 標籤 | 模式 | 上/下拉 | 功能 | 程式碼位置 |
|------|------|------|---------|------|-----------|
| PD7 | `LED` | Output PP（低速） | 無 | 指示燈；初始化點亮，主迴圈每 1 s 切換 | `main.h:60-61`、`gpio.c:54,60-64`、`main.c:97,134` |
| PB4 | `RF_M0` | Output PP（低速） | 無 | RF 模組 M0 模式腳；初始化拉低 | `main.h:62-63`、`gpio.c:57,66-71`、`main.c:94` |
| PB5 | `RF_M1` | Output PP（低速） | 無 | RF 模組 M1 模式腳；初始化拉低 | `main.h:64-65`、`gpio.c:57,66-71`、`main.c:95` |
| PE0 | `RF_AUX` | Input（無上拉） | 無 | RF 模組 AUX 狀態腳；主迴圈讀取 | `main.h:66-67`、`gpio.c:73-77`、`main.c:108` |

### 2.2 已配置未使用

| 引腳 | 標籤 | 模式 | 功能 | 說明 |
|------|------|------|------|------|
| PA13 | —（SYS_JTMS-SWDIO） | Serial Wire | SWDIO | 除錯/下載介面，非應用功能（`.ioc` 之 SYS IP） |
| PA14 | —（SYS_JTCK-SWCLK） | Serial Wire | SWCLK | 除錯/下載介面，非應用功能（`.ioc` 之 SYS IP） |
| PB3 | — | （JTAG 已釋放） | JTDO | `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 後釋放為 GPIO，本工程未使用（`stm32f1xx_hal_msp.c:76`） |
| PA15 | — | （JTAG 已釋放） | JTDI | 同上，釋放後未使用 |

> 註：`HAL_MspInit` 呼叫 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 關閉 JTAG、保留 SWD，
> 使 PB3 / PB4 / PA15 自 JTAG 功能釋放；其中僅 PB4 被用作 `RF_M0`。

---

## 3. USART 配置

### 3.1 已實作

| 項目 | 值 |
|------|-----|
| 外設 | USART1（`huart1`） |
| 鮑率 / 資料位 / 停止位 / 校驗 | 9600 / 8 / 1 / 無 |
| 模式 / 流控 / 取樣 | TX_RX / 無 / 16 倍取樣 |
| 重映射 | `__HAL_AFIO_REMAP_USART1_ENABLE()` → 映射至 PB6/PB7（`usart.c:86`） |

| 引腳 | 訊號 | 模式 | 狀態 |
|------|------|------|------|
| PB6 | USART1_TX | AF Push-Pull（高速） | 【已實作】`main.c:126` 以 `HAL_UART_Transmit` 送出 `CAR_TEST,%lu,AUX=%d\r\n` |
| PB7 | USART1_RX | Input（無上拉） | 【已配置未使用】`usart.c:81-84` 已配置 RX，但全工程無任何 `HAL_UART_Receive*` 呼叫 |

---

## 4. ADC 配置

**狀態：【板級已確認、尚未配置】**（硬體走線已確認，`.ioc` 尚未改動）

板級走線確認如下（7 路類比輸入，全在 ADC1）：

| 引腳 | 板級標籤 | ADC 通道 | 引腳 | 板級標籤 | ADC 通道 |
|------|----------|----------|------|----------|----------|
| PA0 | EXAD0 | ADC1_IN0 | PA4 | EXAD4 | ADC1_IN4 |
| PA1 | EXAD1 | ADC1_IN1 | PA5 | EXCC_AD0 | ADC1_IN5 |
| PA2 | EXAD2 | ADC1_IN2 | PA6 | EXCC_AD1 | ADC1_IN6 |
| PA3 | EXAD3 | ADC1_IN3 | | | |

- 通道編號與引腳連續對應（PA0~PA6 = IN0~IN6），適合單一 ADC1 掃描。
- **無衝突**：PA0~PA6 未與 PB4/PB5/PB6/PB7/PE0/PD7/SWD 重疊。
- 現況：`.ioc` 無 ADC IP、`stm32f1xx_hal_conf.h:37` 之 `HAL_ADC_MODULE_ENABLED` 為註解、無 `adc.c`/`adc.h`。
- 規劃之 CubeMX 配置見第 8 節。

---

## 5. CAN 配置

**狀態：【板級已確認、尚未配置】**（硬體走線已確認，`.ioc` 尚未改動）

| 外設 | 訊號 | 引腳 | 模式 |
|------|------|------|------|
| CAN1 | RX | PA11 | 預設（無 remap） |
| CAN1 | TX | PA12 | 預設（無 remap） |
| CAN2 | RX | PB12 | 預設（無 remap） |
| CAN2 | TX | PB13 | 預設（無 remap） |

- **無衝突**：PA11/PA12、PB12/PB13 均未與現有功能重疊。
- 現況：`.ioc` 無 CAN IP、`stm32f1xx_hal_conf.h:39` 之 `HAL_CAN_MODULE_ENABLED` 為註解、無 `can.c`/`can.h`。
- 規劃之 CubeMX 配置見第 8 節。

---

## 6. 時脈 / 系統配置

| 項目 | 值 | 程式碼位置 |
|------|-----|-----------|
| 時脈源 | HSE 8 MHz 外部晶振 | `main.c:156-163`、`stm32f1xx_hal_conf.h:87` |
| PLL | HSE × 9 → 72 MHz | `main.c:162-163` |
| SYSCLK | 72 MHz（PLLCLK） | `main.c:174` |
| AHB | 72 MHz（DIV1） | `main.c:175` |
| APB1 | 36 MHz（DIV2，CAN 時脈源） | `main.c:176` |
| APB2 | 72 MHz（DIV1，ADC 時脈源） | `main.c:177` |
| Flash 延遲 | 2 wait states | `main.c:179` |

> 觀察項：`main.c:186` 存在 `__HAL_RCC_PLLI2S_ENABLE();`（CubeMX 生成之殘留，
> 本工程未配置 I2S），不影響目前 USART/GPIO 功能，建議後續視需求確認是否移除。

---

## 7. 總結：狀態清單

| 分類 | 項目 |
|------|------|
| 【已實作】 | PD7（LED）、PB4（RF_M0）、PB5（RF_M1）、PE0（RF_AUX）、PB6（USART1_TX）、HSE/時脈樹 |
| 【已配置未使用】 | PB7（USART1_RX）、PA13/PA14（SWD 除錯）、PB3/PA15（JTAG 釋放後閒置） |
| 【板級已確認】 | ADC1：PA0~PA6（7 路，IN0~IN6）；CAN1：PA11/PA12；CAN2：PB12/PB13 |
| 【尚未實作】 | USB OTG、I2C、SPI、Timer、DAC 等（均未在 `.ioc` 啟用） |

---

## 8. 規劃中的 CubeMX 配置（尚未寫入 `.ioc`）

> 基於現有可編譯基線規劃；本節僅為規劃，`.ioc` 與程式碼尚未修改。

### 8.1 ADC1 — 7 通道掃描 + DMA

| 項目 | 建議值 |
|------|--------|
| 外設 | ADC1（IN0~IN6，共 7 rank） |
| 時脈 prescaler | **PCLK2 ÷ 6 = 12 MHz**（F1 ADC 上限 14 MHz；現 `.ioc` 隱含 36 MHz 不可用） |
| Resolution | 12-bit |
| Data Alignment | Right |
| Scan Conversion Mode | Enabled |
| Continuous Conversion Mode | Enabled |
| Discontinuous Mode | Disabled |
| DMA Continuous Requests | Enabled |
| Number Of Conversion | 7（Rank1=IN0 … Rank7=IN6） |
| Sampling Time | 55.5 cycles（高阻分壓源可提高至 239.5） |
| DMA | DMA1_Channel1（ADC1 專用）、Peripheral→Memory、Circular、HalfWord（16-bit）、Memory 遞增 |

### 8.2 CAN1 — PA11/PA12

| 項目 | 建議值 |
|------|--------|
| 模式 | Normal（非 Loopback）、無 remap |
| 時脈源 | PCLK1 = 36 MHz |
| 500 kbps | Prescaler=8、BS1=6、BS2=2、SJW=1（取樣點 77.8%） |
| 250 kbps | Prescaler=8、BS1=13、BS2=4、SJW=1（取樣點 77.8%） |

### 8.3 CAN2 — PB12/PB13

| 項目 | 建議值 |
|------|--------|
| 模式 | Normal、無 remap |
| Bit timing | 同 CAN1 |
| 注意 | CAN2 為 CAN1 之從屬，需先初始化 CAN1 並共用過濾器 bank |

### 8.4 NVIC / 預計產生的程式碼

- 中斷（視需求）：ADC1 global interrupt、DMA1 Channel1 global interrupt、CAN1/CAN2 TX/RX0/RX1。
- 啟用 HAL 模組：`HAL_ADC_MODULE_ENABLED`、`HAL_CAN_MODULE_ENABLED`（`stm32f1xx_hal_conf.h`）。
- 產生檔案：`adc.c`/`adc.h`、`can.c`/`can.h`、`dma.c`/`dma.h`。
- 初始化函式：`MX_ADC1_Init()`、`MX_CAN1_Init()`、`MX_CAN2_Init()`、`MX_DMA_Init()`。
- Keil 工程新增：`stm32f1xx_hal_adc.c`、`stm32f1xx_hal_can.c`。
