# PHASE1 Notes — ADC1 7-ch DMA + CAN1/CAN2 Loopback

> 基線狀態：Keil MDK-ARM（ARMCLANG V6.24）實際 Build **0 Error / 0 Warning**。
> 本階段不開啟 CAN Normal 模式、不新增 CAN/ADC 中斷；USART1/RF 功能原樣保留。

## 1. Build 結果

| 項目 | 值 |
|------|-----|
| 工具鏈 | ARMCLANG V6.24（AC6） |
| 結果 | **0 Error / 0 Warning** |
| Code / RO / RW / ZI | 15824 / 1104 / 12 / 1932 bytes |
| 產物 | `MDK-ARM\AEBS_RF_Test\AEBS_RF_Test.axf`、`AEBS_RF_Test.hex` |

## 2. 時鐘配置

| 項目 | 值 |
|------|-----|
| HSE | 8 MHz（外部晶振） |
| PLL | ×9 → 72 MHz |
| SYSCLK / AHB | 72 MHz |
| APB1（CAN 時脈源） | 36 MHz |
| APB2（ADC 時脈源） | 72 MHz |
| **ADCCLK** | **PCLK2 / 6 = 12 MHz**（`__HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6)`，於 `adc.c` 之 `MX_ADC1_Init()` 內設定） |

> F1 ADC 時脈上限 14 MHz，故採用 /6（12 MHz）。

## 3. ADC1 — 7 通道掃描 + DMA

| 項目 | 配置 |
|------|------|
| 外設 | ADC1 |
| 通道 | PA0~PA6 = EXAD0~4、EXCC_AD0~1 → ADC_IN0~IN6 |
| Rank | Rank1=IN0 … Rank7=IN6（依序） |
| Scan / Continuous | ENABLE / ENABLE |
| 觸發 | ADC_SOFTWARE_START（軟體） |
| 對齊 / 解析度 | Right / 12-bit |
| 取樣時間 | 55.5 cycles |
| DMA | DMA1_Channel1、Periph→Mem、Circular、HalfWord、MemInc |
| 目標緩衝 | `uint16_t adc_raw[7]`（`adc.c` 全域） |
| 啟動 | `ADC1_StartContinuous(adc_raw, 7)` → `HAL_ADC_Start_DMA` |

## 4. CAN1 / CAN2 — Loopback 自測

| 項目 | CAN1 | CAN2 |
|------|------|------|
| 腳位 | PA11(RX) / PA12(TX) | PB12(RX) / PB13(TX) |
| 模式 | CAN_MODE_LOOPBACK | CAN_MODE_LOOPBACK |
| Prescaler / BS1 / BS2 / SJW | 8 / 6TQ / 2TQ / 1TQ | 同左 |
| 理論 bitrate | 500 kbps @36MHz | 同左 |
| Filter bank | 0 | 14 |
| SlaveStartFilterBank | 14 | 14 |
| 驗證 | `CAN_LoopbackTest(&hcan1, 0x321, payload, 8)` | `CAN_LoopbackTest(&hcan2, 0x456, payload, 8)` |

- 雙 CAN 共用 28 個 filter bank：CAN1 佔 0~13、CAN2 佔 14~27（`SlaveStartFilterBank=14`）。
- 自測方式：TX 一筆標準資料幀 → 輪詢 RX FIFO0 → 比對 StdId/DLC/payload，回傳 HAL_OK 即 PASS。
- CAN2 為 CAN1 之從屬，初始化順序為 CAN1 → CAN2，啟動亦同。

## 5. GPIO 現況（本階段未變動既有功能）

| 引腳 | 功能 | 狀態 |
|------|------|------|
| PD7 | LED | 已實作（1 s 切換） |
| PB4 | RF_M0 | 已實作 |
| PB5 | RF_M1 | 已實作 |
| PE0 | RF_AUX | 已實作 |
| PB6/PB7 | USART1_TX/RX（remap） | 已實作/已配置未使用 RX |
| PA13/PA14 | SWDIO/SWCLK | 除錯 |
| PA0~PA6 | ADC1_IN0~IN6 | 本階段新增（analog） |
| PA11/PA12 | CAN1_RX/TX | 本階段新增（AF） |
| PB12/PB13 | CAN2_RX/TX | 本階段新增（AF） |

## 6. Debug Log（USART1，9600 8N1）

開機一次性輸出：
- `[ADC] init OK, DMA circular started`（或 `FAILED`）
- `[CAN] filter config FAILED`（若失敗）
- `[CAN] init OK (CAN1/CAN2 loopback)`（或 `start FAILED`）
- `[CAN1] loopback PASS/FAIL`
- `[CAN2] loopback PASS/FAIL`

主迴圈週期輸出（每 1 s）：
- RF：`CAR_TEST,%lu,AUX=%d\r\n`
- ADC：`ADC,%u,%u,%u,%u,%u,%u,%u\r\n`（adc_raw[0..6]）

## 7. 新增檔案

| 檔案 | 說明 |
|------|------|
| `Core/Inc/adc.h` / `Core/Src/adc.c` | ADC1 init + MSP + 啟動函式 |
| `Core/Inc/can.h` / `Core/Src/can.c` | CAN1/CAN2 init + filter + loopback 自測 |
| `Core/Inc/dma.h` / `Core/Src/dma.c` | `hdma_adc1`（DMA1_Channel1） |
| `Drivers/.../Src/stm32f1xx_hal_adc.c` | 複製自 STM32Cube FW_F1 V1.8.7 |
| `Drivers/.../Src/stm32f1xx_hal_adc_ex.c` | 複製（供 ADCEx weak callback，解決 L6218E） |
| `Drivers/.../Src/stm32f1xx_hal_can.c` | 複製自 STM32Cube FW_F1 V1.8.7 |
| `Drivers/.../Inc/stm32f1xx_hal_adc.h` / `_adc_ex.h` / `_can.h` | 對應標頭 |

## 8. 修改檔案

| 檔案 | 修改 |
|------|------|
| `AEBS_RF_Test.ioc` | 新增 ADC1/CAN1/CAN2/DMA IP（`Mcu.IPNb=8`）、腳位（`PinsNb=22`）、ADC DIV6、CAN loopback、DMA request；`RCC.ADCFreqValue=12000000` |
| `Core/Inc/stm32f1xx_hal_conf.h` | 啟用 `HAL_ADC_MODULE_ENABLED`、`HAL_CAN_MODULE_ENABLED` |
| `Core/Src/main.c` | 新增 init 呼叫、`dbg_log()`、CAN 自測、ADC 週期輸出（保留 RF `CAR_TEST`） |
| `MDK-ARM/AEBS_RF_Test.uvprojx` | 新增 `adc.c/can.c/dma.c` 與 `hal_adc.c/hal_adc_ex.c/hal_can.c` |

---

# 實板 Bring-up Checklist

> 依序執行，前一步 PASS 再進下一步。

### ① 供電檢查
- [ ] 以萬用表量測 MCU VDD/VDDA 對 GND 電壓（應為 3.3 V）。
- [ ] 確認 VDDA/VSSA 供電穩定（ADC 參考電壓源）。
- [ ] 確認板上無短路（上電前量 VDD 對 GND 阻抗，非 0Ω）。
- [ ] 量測 HSE 晶振（8 MHz）起振正常（示波器或量 72 MHz 時脈輸出）。

### ② ST-Link 連線 / 燒錄
- [ ] ST-Link 連接 SWD（PA13=SWDIO、PA14=SWCLK、GND、3V3）。
- [ ] Keil 內 `Flash -> Download` 燒錄 `AEBS_RF_Test.hex` 成功。
- [ ] 燒錄後 Reset，確認程式不卡在 `Error_Handler()`（LED 應閃爍）。

### ③ USART1 串口輸出（binary packet）
- [ ] 串口工具（9600 8N1）接 PB6(TX)/PB7(RX)，GND 共地，確認開機 banner：
      `[ADC] init OK...`、`[CAN] init OK...`、`[CAN1/CAN2] loopback ...`。
- [ ] 關閉一般串口工具，改用 PC 端解析器接收 binary packet：
      `python tools/serial_parser.py --port COMx --baud 9600 --csv out.csv`
- [ ] 確認每 1 s 收到一筆封包、無 CRC fail、CSV 正常寫入（mock 模式 adc0≈1000 起遞增）。

### ④ CAN1 / CAN2 Loopback PASS/FAIL
- [ ] 串口顯示 `[CAN1] loopback PASS`。
- [ ] 串口顯示 `[CAN2] loopback PASS`。
- [ ] 若 FAIL：檢查 CAN 時鐘（36 MHz）、filter 配置、CAN1→CAN2 初始化順序。

### ⑤ ADC 7 路 Raw 值驗證
- [ ] 串口週期輸出 `ADC,a,b,c,d,e,f,g`（對應 EXAD0~4、EXCC_AD0~1）。
- [ ] 確認 `adc_raw[]` 由 DMA circular 持續更新（數值不凍結）。
- [ ] 逐一對 7 路輸入施加信號，確認數值隨電壓線性變化、無串音。

**測試點須區分兩者，對應 raw 值不同：**

**(A) 直接量測 MCU ADC pin（PA0~PA6）**
- [ ] 直接於 PA0~PA6 施加 0V / 1.65V / 3.3V。
- [ ] 確認 raw ≈ 0 / 2048 / 4095（12-bit，容許誤差）。

**(B) 板級外部輸入口測試（EXAD0~4、EXCC_AD0~1）**
- [ ] 於板上 EXAD0~4 / EXCC_AD0~1 端子施加電壓（訊號經約 **1.74kΩ / 3.32kΩ 分壓**，分壓比 ≈ 3.32/(1.74+3.32) ≈ 0.656 後進入 ADC pin）。
- [ ] 確認外部 0V / 1.65V / 3.3V / 5V 約對應 raw **0 / 1343 / 2687 / 4071**（容許誤差）。

> 註：0/2048/4095 僅適用於直接量 MCU ADC 腳；從板級外部口測試必須乘上分壓比，勿直接以 0/2048/4095 判定。

---

### 後續（本階段明確不執行）
- 不將 CAN 切至 `CAN_MODE_NORMAL`（需外部收發器 + 120Ω 終端 + 與對端一致 bitrate）。
- 不新增 CAN/ADC 中斷（目前 ADC/CAN 皆輪詢）。
- 不變動已通過 0 Error / 0 Warning 的基線。
