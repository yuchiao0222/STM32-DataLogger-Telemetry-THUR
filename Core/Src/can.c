/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration of the CAN instances
  *          and for polling received frames from the vehicle CAN bus.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;
CAN_HandleTypeDef hcan2;

/* CAN1 init function */
void MX_CAN1_Init(void)
{
  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 8;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */
}

/* CAN2 init function */
void MX_CAN2_Init(void)
{
  /* USER CODE BEGIN CAN2_Init 0 */

  /* USER CODE END CAN2_Init 0 */

  /* USER CODE BEGIN CAN2_Init 1 */

  /* USER CODE END CAN2_Init 1 */
  hcan2.Instance = CAN2;
  hcan2.Init.Prescaler = 8;
  hcan2.Init.Mode = CAN_MODE_NORMAL;
  hcan2.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan2.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan2.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan2.Init.TimeTriggeredMode = DISABLE;
  hcan2.Init.AutoBusOff = ENABLE;
  hcan2.Init.AutoWakeUp = DISABLE;
  hcan2.Init.AutoRetransmission = ENABLE;
  hcan2.Init.ReceiveFifoLocked = DISABLE;
  hcan2.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN2_Init 2 */

  /* USER CODE END CAN2_Init 2 */
}

/**
  * @brief Configure shared filter banks for dual CAN.
  *        28 banks shared: CAN1 (master) owns bank 0..13, CAN2 (slave) 14..27.
  *        Both banks are set to "accept all" (ID mask mode, mask = 0) so that
  *        every frame reaches software, where the DBC ID match is performed.
  * @retval HAL status
  */
HAL_StatusTypeDef CAN_ConfigFilters(void)
{
  CAN_FilterTypeDef sFilterConfig;
  HAL_StatusTypeDef status;

  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;

  sFilterConfig.FilterBank = 0;
  sFilterConfig.SlaveStartFilterBank = 14;
  status = HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);
  if (status != HAL_OK)
  {
    return status;
  }

  sFilterConfig.FilterBank = 14;
  status = HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig);
  if (status != HAL_OK)
  {
    return status;
  }

  return HAL_OK;
}

/**
  * @brief Start CAN1 and CAN2 (CAN2 is slave, CAN1 must be started first).
  * @retval HAL status
  */
HAL_StatusTypeDef CAN_Start(void)
{
  HAL_StatusTypeDef status;

  status = HAL_CAN_Start(&hcan1);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_CAN_Start(&hcan2);
  if (status != HAL_OK)
  {
    return status;
  }

  return HAL_OK;
}

/**
  * @brief  Poll CAN1 then CAN2 RX FIFO0 for the next received frame.
  * @param  msg  Output frame.
  * @retval 1 if a frame was received, 0 otherwise.
  */
int CAN_RxPoll(can_rx_msg_t *msg)
{
  CAN_RxHeaderTypeDef rx;
  uint8_t data[8] = {0};
  HAL_StatusTypeDef status;

  if (msg == NULL)
  {
    return 0;
  }

  if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0U)
  {
    status = HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rx, data);
    if (status == HAL_OK)
    {
      msg->ide = (rx.IDE == CAN_ID_EXT) ? 1U : 0U;
      msg->id  = (rx.IDE == CAN_ID_EXT) ? rx.ExtId : rx.StdId;
      msg->dlc = (uint8_t)rx.DLC;
      for (uint8_t i = 0U; i < 8U; i++)
      {
        msg->data[i] = (i < msg->dlc) ? data[i] : 0U;
      }
      return 1;
    }
  }

  if (HAL_CAN_GetRxFifoFillLevel(&hcan2, CAN_RX_FIFO0) > 0U)
  {
    status = HAL_CAN_GetRxMessage(&hcan2, CAN_RX_FIFO0, &rx, data);
    if (status == HAL_OK)
    {
      msg->ide = (rx.IDE == CAN_ID_EXT) ? 1U : 0U;
      msg->id  = (rx.IDE == CAN_ID_EXT) ? rx.ExtId : rx.StdId;
      msg->dlc = (uint8_t)rx.DLC;
      for (uint8_t i = 0U; i < 8U; i++)
      {
        msg->data[i] = (i < msg->dlc) ? data[i] : 0U;
      }
      return 1;
    }
  }

  return 0;
}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
  else if(canHandle->Instance==CAN2)
  {
  /* USER CODE BEGIN CAN2_MspInit 0 */

  /* USER CODE END CAN2_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_CAN2_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN2 GPIO Configuration
    PB12     ------> CAN2_RX
    PB13     ------> CAN2_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN2_MspInit 1 */

  /* USER CODE END CAN2_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
  else if(canHandle->Instance==CAN2)
  {
  /* USER CODE BEGIN CAN2_MspDeInit 0 */

  /* USER CODE END CAN2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN2_CLK_DISABLE();

    /**CAN2 GPIO Configuration
    PB12     ------> CAN2_RX
    PB13     ------> CAN2_TX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12|GPIO_PIN_13);

  /* USER CODE BEGIN CAN2_MspDeInit 1 */

  /* USER CODE END CAN2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
