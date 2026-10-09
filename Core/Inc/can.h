/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.h
  * @brief   This file contains all the function prototypes for the can.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/* A received CAN frame, normalized to (ide, id, data). */
typedef struct
{
  uint32_t ide;       /* 0 = standard (11-bit), 1 = extended (29-bit)        */
  uint32_t id;        /* StdId or ExtId                                       */
  uint8_t  data[8];
  uint8_t  dlc;
} can_rx_msg_t;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_CAN1_Init(void);
void MX_CAN2_Init(void);
HAL_StatusTypeDef CAN_ConfigFilters(void);
HAL_StatusTypeDef CAN_Start(void);

/**
  * @brief  Poll CAN1 and CAN2 RX FIFO0 for the next received frame.
  * @param  msg  Output: the received frame (ide/id/data/dlc).
  * @retval 1 if a frame was received, 0 if both FIFOs are empty.
  */
int CAN_RxPoll(can_rx_msg_t *msg);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */
