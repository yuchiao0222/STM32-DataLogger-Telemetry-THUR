/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app.h
  * @brief   Data-acquisition application layer.
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

/* -------------------------------------------------------------------------- */
/* Build switch                                                              */
/*   1 = synthetic test data (offline chain verification, no sensor required) */
/*   0 = real ADC / CAN / RF hardware data                                     */
/* -------------------------------------------------------------------------- */
#define APP_MOCK_MODE   1

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
