

[賽車] ──CAN──> [發射端 STM32 板] ──2.4GHz RF──> [接收端 AS32+USB-TTL] ──> [場外人本機 WebUI]


## ：離車測試（工廠/實驗室，無車）

**目的**：驗證無線鏈路 + 硬體正常，不需要真實 CAN 數據。

### 發射端：不用改，直接上電

- `CAN_USE_LOOPBACK` 保持 `1`（預設，離車自測模式）
- 上電自動發射：測試計數 + CAN loopback 自測結果

### 接收端：直接啟動

```powershell
cd <專案目錄>
python tools\webui_server.py --port COM3 --baud 9600 --web 8080
```

### 驗證

- 本機瀏覽器 `http://localhost:8080`
- 頂部「測試」數字持續遞增 ＝ 通訊正常
- 溫度/SOC 顯示 `--`（正常，離車無 CAN 訊號）

**結論：情境一完全不用改任何配置。**

---

## 情境二：上車（正式採集車上數據）

**目的**：收車上 CAN 總線的馬達溫度、控制器溫度、電池 SOC。

### 發射端：改 1 行 + 確認 3 項，然後重燒

**第 1 步 — 改 CAN 模式**（`Core/Inc/can.h`）：

```c
#define CAN_USE_LOOPBACK  0U   /* 0 = Normal，上車用 */
```

**第 2 步 — 確認車上 CAN 參數**（對照車上 DBC/文檔，不一致要改）：

| 項目 | 位置 | 預設值 |
|------|------|--------|
| CAN 波特率 | `can.c` 的 Prescaler/BS1/BS2 | 500 kbps |
| 馬達幀 ID | `app.c` 的 `CANID_MOTOR_*_FRAME2` | 0x6A2/0x6B2/0x6C2/0x6D2 |
| BMS SOC ID | `app.c` 的 `CANID_BMS_HCU_INFO` | 0x186040F3 |

**第 3 步 — 重新編譯 + 燒錄**：

```powershell
# 編譯（Keil 命令列）
& "D:\Keil\Keil_v5\UV4\UV4.exe" -b "MDK-ARM\AEBS_RF_Test.uvprojx" -j0

# 燒錄（CubeProgrammer）
$prog = "D:\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
& $prog -c port=SWD mode=UR -w "MDK-ARM\AEBS_RF_Test\AEBS_RF_Test.hex" -v
& $prog -c port=SWD mode=UR -rst
```

### 接收端：不用改

### 驗證

- 「測試」數字遞增 ＝ 無線通訊正常
- 馬達/控制器溫度、SOC 開始有值（上車後）

---

## 情境三：接收端日常使用（每次要收數據時）

**不需要重新設定，只要啟動：**

```powershell
# 1. 確認 COM 口（USB-TTL 插著）
Get-PnpDevice -Class Ports | Where-Object {$_.FriendlyName -match 'COM'} | Select-Object FriendlyName

# 2. 啟動 WebUI
cd <專案目錄>
python tools\webui_server.py --port COM3 --baud 9600 --web 8080

# 3. 本機瀏覽器開
#    http://localhost:8080
```

> 場外人就是拿接收端電腦的人，直接看本機 `localhost:8080`，**不需要 IP 設定、不需要網路**。

---

## 情境四：USB-TTL 換孔 / 換接收端電腦

### 只需改 COM 口號

- 查新 COM 口（上面第 1 步），把 `--port COM3` 換成新口號。
- 換新電腦要重裝依賴：

```powershell
pip install pyserial aiohttp
```

### 程式碼不用改、不用重新 git pull（除非發射端有新版本）


---

## 快速速查表

| 我遇到的情況 | 要改什麼 | 怎麼改 |
|-------------|---------|--------|
| 離車測試 | 無 | 直接上電 + 啟動 WebUI |
| 上車採數據 | CAN 開關 + 確認 ID/波特率 | `can.h` 改 0，重編重燒 |
| 下次再測 | 無 | 直接啟動 WebUI |
| USB 換孔 | COM 口 | `--port` 換新號 |
| 換接收端電腦 | COM 口 + 裝依賴 | `pip install pyserial aiohttp` |
| 換網路/斷網 | 無 | 不用改 |
| 換場外人（仍同台電腦） | 無 | 開 `localhost:8080` 即可 |

---

## 附：固定不變的設定（除非硬件改動，永遠別動）

| 項目 | 值 |
|------|-----|
| 無線串口 | 9600 8N1 |
| RF 模式 | M0=0、M1=0（透傳） |
| RF 頻道/地址 | 兩端出廠預設一致 |
| WebUI 埠 | 8080 |
| 協議 | TLV + CRC16，幀頭 0xA5 0x5A |
| 發射週期 | 500ms |
