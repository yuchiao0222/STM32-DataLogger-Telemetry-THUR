/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app.c
  * @brief   Data-acquisition application layer: collects ADC / CAN / RF data,
  *          packs it into the binary telemetry frame and transmits it over
  *          USART1 (RF transparent link).
  *
  *          APP_MOCK_MODE (in app.h) selects synthetic test data for offline
  *          verification of the STM32 -> UART -> PC parser -> CSV chain.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app.h"
#include "main.h"
#include "adc.h"
#include "can.h"
#include "gpio.h"
#include "usart.h"
#include "protocol.h"

/* USER CODE BEGIN 0 */
#include <string.h>

/* USER CODE END 0 */

/* Private variables ---------------------------------------------------------*/
static uint16_t s_sequence = 0;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void app_collect(pkt_frame_t *f);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief Initialize application state.
  */
void APP_Init(void)
{
  s_sequence = 0;
}

#if (APP_MOCK_MODE == 1)

/**
  * @brief Fill the frame with deterministic synthetic data (mock mode).
  *        ADC values are a ramp with a small sequence-dependent offset,
  *        CAN/RF fields are fixed patterns.
  */
static void app_collect(pkt_frame_t *f)
{
  uint8_t i;

  for (i = 0U; i < PKT_ADC_CHANNELS; i++)
  {
    f->adc_raw[i] = (uint16_t)(1000U + ((uint32_t)i * 200U) + (s_sequence % 100U));
  }

  f->can_status = CAN_ST_CAN1_RUN | CAN_ST_CAN2_RUN
                | CAN_ST_CAN1_PASS | CAN_ST_CAN2_PASS;

  for (i = 0U; i < PKT_CAN_DATA_LEN; i++)
  {
    f->can1_data[i] = (uint8_t)(0x11U + i);
    f->can2_data[i] = (uint8_t)(0x21U + i);
  }

  f->rf_status = RF_ST_AUX;
}

#else

/**
  * @brief Sample CAN loopback (TX -> RX) and fill can1/can2 data + status.
  * @retval can_status flags
  */
static uint8_t app_can_sample(pkt_frame_t *f)
{
  static const uint8_t payload[PKT_CAN_DATA_LEN] =
    {0x11U,0x22U,0x33U,0x44U,0x55U,0x66U,0x77U,0x88U};
  uint8_t status = 0U;

  if (CAN_LoopbackTest(&hcan1, 0x321U, payload, PKT_CAN_DATA_LEN) == HAL_OK)
  {
    status |= CAN_ST_CAN1_PASS;
    (void)memcpy(f->can1_data, payload, PKT_CAN_DATA_LEN);
  }
  else
  {
    (void)memset(f->can1_data, 0, PKT_CAN_DATA_LEN);
  }

  if (CAN_LoopbackTest(&hcan2, 0x456U, payload, PKT_CAN_DATA_LEN) == HAL_OK)
  {
    status |= CAN_ST_CAN2_PASS;
    (void)memcpy(f->can2_data, payload, PKT_CAN_DATA_LEN);
  }
  else
  {
    (void)memset(f->can2_data, 0, PKT_CAN_DATA_LEN);
  }

  if (hcan1.State == HAL_CAN_STATE_LISTENING)
  {
    status |= CAN_ST_CAN1_RUN;
  }
  if (hcan2.State == HAL_CAN_STATE_LISTENING)
  {
    status |= CAN_ST_CAN2_RUN;
  }

  return status;
}

/**
  * @brief Fill the frame with real hardware data.
  */
static void app_collect(pkt_frame_t *f)
{
  (void)memcpy(f->adc_raw, adc_raw, sizeof(f->adc_raw));

  f->can_status = app_can_sample(f);

  f->rf_status =
    (HAL_GPIO_ReadPin(RF_AUX_GPIO_Port, RF_AUX_Pin) == GPIO_PIN_SET)
    ? RF_ST_AUX : 0x00U;
}

#endif /* APP_MOCK_MODE */

/**
  * @brief Build one telemetry frame and transmit it over USART1.
  */
void APP_Process(void)
{
  pkt_frame_t frame;

  proto_packet_init(&frame);

  frame.sequence = s_sequence++;
  frame.timestamp_ms = (uint32_t)HAL_GetTick();

  app_collect(&frame);

  proto_packet_finalize(&frame);

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)&frame,
                          (uint16_t)proto_packet_size(), 1000U);
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
