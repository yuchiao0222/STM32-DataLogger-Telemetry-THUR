# STM32 DataLogger Telemetry THUR

STM32F105VCT6-based data acquisition and telemetry system for multi-channel analog sampling, dual CAN communication, and wireless data transmission.

## Features

* 7-channel ADC data acquisition
* ADC1 scan mode with DMA circular sampling
* Dual CAN interfaces

  * CAN1: PA11 / PA12
  * CAN2: PB12 / PB13
* CAN loopback self-test
* RF telemetry through USART1
* Binary telemetry packet protocol
* CRC16 packet validation
* PC-side Python packet parser
* CSV data logging
* Mock data mode for offline protocol testing

## Hardware

### MCU

* STM32F105VCT6
* System clock: 72 MHz
* APB1: 36 MHz
* APB2: 72 MHz
* ADC clock: 12 MHz

### ADC

Seven analog input channels are connected to ADC1:

| External Input | MCU Pin | ADC Channel |
| -------------- | ------- | ----------- |
| EXAD0          | PA0     | ADC1_IN0    |
| EXAD1          | PA1     | ADC1_IN1    |
| EXAD2          | PA2     | ADC1_IN2    |
| EXAD3          | PA3     | ADC1_IN3    |
| EXAD4          | PA4     | ADC1_IN4    |
| EXCC_AD0       | PA5     | ADC1_IN5    |
| EXCC_AD1       | PA6     | ADC1_IN6    |

ADC configuration:

* 12-bit resolution
* 7-rank scan
* Continuous conversion
* DMA1 Channel 1
* Circular DMA mode
* Half-word transfer
* Sampling time: 55.5 cycles

The external analog input circuit uses approximately a 1.74 kΩ / 3.32 kΩ voltage divider, allowing approximately 0–5 V external signals to be measured by the 3.3 V ADC.

### CAN

CAN1:

* RX: PA11
* TX: PA12

CAN2:

* RX: PB12
* TX: PB13

CAN transceivers:

* TJA1051T/3

Current firmware configuration:

* CAN mode: Loopback
* CAN clock: 36 MHz
* Prescaler: 8
* BS1: 6 TQ
* BS2: 2 TQ
* SJW: 1 TQ
* Nominal bitrate: 500 kbps

CAN1 and CAN2 currently use internal loopback mode for firmware verification. Real CAN bus communication will be enabled after hardware bring-up.

### RF Telemetry

USART1 is used for RF communication.

* USART1 TX: PB6
* USART1 RX: PB7
* RF_M0: PB4
* RF_M1: PB5
* RF_AUX: PE0
* Baud rate: 9600

The RF module is currently configured for transparent transmission mode.

## Power Architecture

The board uses the following power chain:

```text
12 V Input
    |
    v
TLE8366EV50
    |
    v
5 V
    |
    v
AS1117-3.3V
    |
    v
3.3 V
```

The main power input is 12 V.

## Project Structure

```text
.
├── Core/
│   ├── Inc/
│   │   ├── adc.h
│   │   ├── app.h
│   │   ├── can.h
│   │   ├── dma.h
│   │   ├── gpio.h
│   │   ├── main.h
│   │   ├── protocol.h
│   │   └── usart.h
│   │
│   └── Src/
│       ├── adc.c
│       ├── app.c
│       ├── can.c
│       ├── dma.c
│       ├── gpio.c
│       ├── main.c
│       ├── protocol.c
│       └── usart.c
│
├── Drivers/
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
│
├── MDK-ARM/
│   ├── AEBS_RF_Test.uvprojx
│   └── startup_stm32f105xc.s
│
├── docs/
│   ├── BOARD_PINMAP.md
│   └── PHASE1_NOTES.md
│
├── tools/
│   ├── packet.py
│   └── serial_parser.py
│
├── AEBS_RF_Test.ioc
├── .gitignore
└── README.md
```

## Firmware Architecture

The firmware is divided into several layers:

```text
ADC + DMA ──────┐
                |
CAN1 / CAN2 ────┼──> Application Layer
                |          |
RF Status ──────┘          v
                    Telemetry Protocol
                           |
                           v
                        USART1
                           |
                           v
                       RF Module
```

The application layer collects data from the hardware interfaces and packages it into a binary telemetry packet.

## Telemetry Protocol

The telemetry protocol includes:

* Packet header
* Protocol version
* Sequence number
* Timestamp
* 7-channel ADC raw data
* CAN status/data
* RF status
* CRC16

CRC16 is used to verify packet integrity on the receiving side.

## PC Receiver

The PC-side Python tools are located in:

```text
tools/
```

Install the required serial library if necessary:

```bash
pip install pyserial
```

Run the receiver:

```bash
python tools/serial_parser.py --port COM5 --baud 9600 --csv out.csv
```

Replace `COM5` with the actual COM port of the CH343 USB-UART adapter.

The receiver:

* Reads binary telemetry packets
* Validates CRC16
* Parses ADC and status data
* Displays received information
* Saves received data to CSV

## Current Status

### Completed

* STM32F105 project configuration
* 72 MHz system clock
* USART1 RF communication
* 7-channel ADC configuration
* ADC DMA circular acquisition
* CAN1 initialization
* CAN2 initialization
* CAN loopback self-test
* Binary telemetry protocol
* CRC16
* Application layer
* Mock data mode
* Python packet parser
* CSV logger
* Keil build verification

Current Keil build result:

```text
0 Errors
0 Warnings
```

### Pending Hardware Verification

* 12 V power-on verification
* 5 V and 3.3 V rail verification
* ST-Link programming and debugging
* USART1 communication through CH343
* ADC DMA hardware validation
* 7-channel analog input calibration
* CAN1 loopback hardware execution
* CAN2 loopback hardware execution
* RF telemetry hardware validation
* Real CAN bus testing

### Future Work

* Switch CAN from Loopback mode to Normal mode
* Add CAN interrupt-based reception
* Define real CAN message IDs and decoding rules
* Add ADC calibration
* Convert ADC raw values to physical measurements
* Validate RF communication range and reliability
* Improve PC-side real-time monitoring
* Add data visualization/dashboard
* Perform full vehicle integration testing

## Documentation

Detailed hardware and development notes are available in:

* `docs/BOARD_PINMAP.md`
* `docs/PHASE1_NOTES.md`

## Development Environment

* MCU: STM32F105VCT6
* STM32CubeMX
* STM32CubeF1 HAL
* Keil MDK-ARM
* Python 3
* CH343 USB-UART
* ST-Link V2
