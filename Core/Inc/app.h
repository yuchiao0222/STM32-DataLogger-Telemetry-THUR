/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app.h
  * @brief   Data-acquisition application layer: reads the vehicle CAN bus,
  *          decodes the DBC signals, packs them into the extensible telemetry
  *          protocol and transmits over USART1 (2.4 GHz RF transparent link).
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Telemetry transmission period in milliseconds. */
#define APP_TX_PERIOD_MS    500U

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void APP_Init(void);
void APP_Process(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
