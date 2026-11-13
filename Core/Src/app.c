/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app.c
  * @brief   Data-acquisition application layer.
  *
  *          Reads the vehicle CAN bus, decodes the DBC signals of interest
  *          (motor temperature, motor-controller temperature, battery SOC),
  *          caches the latest value of each, packs them into the extensible
  *          TLV telemetry frame and transmits over USART1 (2.4 GHz RF link).
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app.h"
#include "main.h"
#include "can.h"
#include "usart.h"
#include "protocol.h"

/* USER CODE BEGIN 0 */

/* -------------------------------------------------------------------------- */
/* DBC message IDs (see PNL.dbc / BMS_M79C7_XWD.dbc / IMU_CH110.dbc).        */
/*   MotorAndMCUTemp (PNL, std 11-bit): 8 temps, MCU offset -60, motor -40.   */
/*   PadelAndTrq    (PNL, std 11-bit): VehSpd_kph @ byte[2].                  */
/*   BMS_SYS_IFO    (BMS, ext 29-bit, little-endian):                         */
/*     byte[0..1] = SysVoltage (0.1 V), byte[4] = SOC (%).                    */
/* -------------------------------------------------------------------------- */
#define CANID_MOTOR_MCU_TEMP   226U       /* 0x0E2, MotorAndMCUTemp           */
#define CANID_PADEL_AND_TRQ    1560U      /* 0x618, VehSpd_kph                */
#define CANID_CH110_ACCEL      392U       /* 0x188, CH110_TPDO1 (accel X/Y/Z) */

/* DBC lists the BMS ID in full 32-bit form; the low 29 bits are used on wire. */
#define CANID_BMS_SYS_IFO      0x186040F0U

#define MCU_TEMP_OFFSET_DEGC    (-60)     /* DBC offset for MCU temp           */
#define MOTOR_TEMP_OFFSET_DEGC  (-40)     /* DBC offset for motor temp         */
#define TEMP_SCALE              (10)      /* store 0.1 degC                    */

/* USER CODE END 0 */

/* Private variables ---------------------------------------------------------*/
static uint16_t s_sequence = 0;
static uint32_t s_last_tx_ms = 0;
static uint8_t s_test_counter = 0;
static uint32_t s_can_rx_count = 0;
static uint32_t s_can_last_id = 0;
static uint32_t s_can_ids[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Latest decoded values (physical units) + validity flags. */
static int16_t s_motor_temp[4] = {0, 0, 0, 0};
static int16_t s_ctrl_temp[4]  = {0, 0, 0, 0};
static uint8_t s_soc = 0;
static uint8_t s_motor_temp_valid[4] = {0, 0, 0, 0};
static uint8_t s_ctrl_temp_valid[4]  = {0, 0, 0, 0};
static uint8_t s_soc_valid = 0;

/* Acceleration (milli-g), voltages, motor speed, vehicle speed. */
static int16_t  s_acc_x = 0, s_acc_y = 0, s_acc_z = 0;
static uint16_t s_bat_voltage = 0;   /* 0.1 V */
static uint16_t s_bus_voltage = 0;   /* V     */
static int16_t  s_motor_speed = 0;   /* rpm   */
static uint8_t  s_veh_speed = 0;     /* km/h  */
static uint8_t  s_acc_valid = 0;
static uint8_t  s_bat_volt_valid = 0;
static uint8_t  s_bus_volt_valid = 0;
static uint8_t  s_speed_valid = 0;
static uint8_t  s_veh_speed_valid = 0;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void app_decode(const can_rx_msg_t *m);
static void app_transmit(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief Initialize application state.
  */
void APP_Init(void)
{
  s_sequence = 0;
  s_last_tx_ms = 0U;
  s_soc_valid = 0;

  for (uint8_t i = 0U; i < 4U; i++)
  {
    s_motor_temp_valid[i] = 0;
    s_ctrl_temp_valid[i] = 0;
  }

  /* Boot-time protocol self-test (fixed golden vector, no hardware needed). */
  (void)proto_selftest();
}

/**
  * @brief Decode one received CAN frame and update the latest-value cache.
  */
static void app_decode(const can_rx_msg_t *m)
{
  if (m->ide == 0U)
  {
    /* Standard 11-bit frames. */
    switch (m->id)
    {
      case CANID_MOTOR_MCU_TEMP:
        /* MotorAndMCUTemp: 8 temps (FL/RL/FR/RR), MCU offset -60, motor -40. */
        if (m->dlc >= 8U)
        {
          s_ctrl_temp[0]  = (int16_t)(((int32_t)m->data[0] + MCU_TEMP_OFFSET_DEGC)   * TEMP_SCALE);
          s_motor_temp[0] = (int16_t)(((int32_t)m->data[1] + MOTOR_TEMP_OFFSET_DEGC) * TEMP_SCALE);
          s_ctrl_temp[1]  = (int16_t)(((int32_t)m->data[2] + MCU_TEMP_OFFSET_DEGC)   * TEMP_SCALE);
          s_motor_temp[1] = (int16_t)(((int32_t)m->data[3] + MOTOR_TEMP_OFFSET_DEGC) * TEMP_SCALE);
          s_ctrl_temp[2]  = (int16_t)(((int32_t)m->data[4] + MCU_TEMP_OFFSET_DEGC)   * TEMP_SCALE);
          s_motor_temp[2] = (int16_t)(((int32_t)m->data[5] + MOTOR_TEMP_OFFSET_DEGC) * TEMP_SCALE);
          s_ctrl_temp[3]  = (int16_t)(((int32_t)m->data[6] + MCU_TEMP_OFFSET_DEGC)   * TEMP_SCALE);
          s_motor_temp[3] = (int16_t)(((int32_t)m->data[7] + MOTOR_TEMP_OFFSET_DEGC) * TEMP_SCALE);
          for (uint8_t i = 0U; i < 4U; i++)
          {
            s_ctrl_temp_valid[i] = 1U;
            s_motor_temp_valid[i] = 1U;
          }
        }
        return;
      case CANID_PADEL_AND_TRQ:
        /* VehSpd_kph @ byte[2], unit km/h. */
        if (m->dlc >= 3U)
        {
          s_veh_speed = m->data[2];
          s_veh_speed_valid = 1U;
        }
        return;
      case CANID_CH110_ACCEL:
        /* Acc_X/Y/Z: 16-bit little-endian signed, unit milli-g. */
        if (m->dlc >= 6U)
        {
          s_acc_x = (int16_t)((uint16_t)m->data[0] | ((uint16_t)m->data[1] << 8));
          s_acc_y = (int16_t)((uint16_t)m->data[2] | ((uint16_t)m->data[3] << 8));
          s_acc_z = (int16_t)((uint16_t)m->data[4] | ((uint16_t)m->data[5] << 8));
          s_acc_valid = 1U;
        }
        return;
      default: return;
    }
  }
  else
  {
    /* Extended 29-bit: BMS_SYS_IFO (system voltage + SOC). */
    if (m->id == CANID_BMS_SYS_IFO)
    {
      if (m->dlc >= 5U)
      {
        /* SysVoltage: 16-bit little-endian @ byte0..1, unit 0.1 V. */
        s_bat_voltage = (uint16_t)m->data[0] | ((uint16_t)m->data[1] << 8);
        s_bat_volt_valid = 1U;
        s_soc = m->data[4];
        s_soc_valid = 1U;
      }
    }
  }
}

/**
  * @brief Build one TLV telemetry frame and transmit it over USART1.
  */
static void app_transmit(void)
{
  pkt_t frame;
  uint8_t i;

  pkt_begin(&frame);
  pkt_set_seq(&frame, s_sequence++);
  pkt_set_ts(&frame, (uint32_t)HAL_GetTick());

  for (i = 0U; i < 4U; i++)
  {
    if (s_motor_temp_valid[i] != 0U)
    {
      (void)pkt_add_i16(&frame, (uint8_t)(TAG_MOTOR_A_TEMP + i), s_motor_temp[i]);
    }
    if (s_ctrl_temp_valid[i] != 0U)
    {
      (void)pkt_add_i16(&frame, (uint8_t)(TAG_CTRL_A_TEMP + i), s_ctrl_temp[i]);
    }
  }

  if (s_soc_valid != 0U)
  {
    (void)pkt_add_u8(&frame, TAG_BAT_SOC, s_soc);
  }

  if (s_acc_valid != 0U)
  {
    (void)pkt_add_i16(&frame, TAG_ACC_X, s_acc_x);
    (void)pkt_add_i16(&frame, TAG_ACC_Y, s_acc_y);
    (void)pkt_add_i16(&frame, TAG_ACC_Z, s_acc_z);
  }
  if (s_bat_volt_valid != 0U)
  {
    (void)pkt_add_u16(&frame, TAG_BAT_VOLTAGE, s_bat_voltage);
  }
  if (s_bus_volt_valid != 0U)
  {
    (void)pkt_add_u16(&frame, TAG_BUS_VOLTAGE, s_bus_voltage);
  }
  if (s_speed_valid != 0U)
  {
    (void)pkt_add_i16(&frame, TAG_MOTOR_SPEED, s_motor_speed);
  }

  if (s_veh_speed_valid != 0U)
  {
    (void)pkt_add_u8(&frame, TAG_VEH_SPEED, s_veh_speed);
  }

  /* Test counter: increments every packet, used to verify the RF link. */
  (void)pkt_add_u8(&frame, TAG_TEST, s_test_counter++);

  /* Debug: total CAN frames received so far. */
  (void)pkt_add_u32(&frame, TAG_CAN_RX_COUNT, s_can_rx_count);

  /* Debug: CAN loopback test result (bit0=CAN1, bit1=CAN2). */
  (void)pkt_add_u8(&frame, TAG_CAN_TEST, g_can_test_result);

  /* Debug: last received CAN ID. */
  (void)pkt_add_u32(&frame, TAG_CAN_LAST_ID, s_can_last_id);

  /* Debug: distinct CAN IDs seen so far. */
  (void)pkt_add_u32(&frame, TAG_CAN_ID0, s_can_ids[0]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID1, s_can_ids[1]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID2, s_can_ids[2]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID3, s_can_ids[3]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID4, s_can_ids[4]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID5, s_can_ids[5]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID6, s_can_ids[6]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID7, s_can_ids[7]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID8, s_can_ids[8]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID9, s_can_ids[9]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID10, s_can_ids[10]);
  (void)pkt_add_u32(&frame, TAG_CAN_ID11, s_can_ids[11]);

  pkt_end(&frame);

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)pkt_data(&frame),
                          (uint16_t)pkt_size(&frame), 1000U);
}

/**
  * @brief Poll CAN and, every APP_TX_PERIOD_MS, send one telemetry frame.
  */
void APP_Process(void)
{
  can_rx_msg_t msg;

  /* Drain all pending CAN frames into the latest-value cache. */
  while (CAN_RxPoll(&msg) == 1)
  {
    uint8_t k;

    s_can_rx_count++;
    s_can_last_id = msg.id;

    /* Record distinct CAN IDs (up to 12 slots). */
    for (k = 0U; k < 12U; k++)
    {
      if (s_can_ids[k] == msg.id)
      {
        break;
      }
      if (s_can_ids[k] == 0U)
      {
        s_can_ids[k] = msg.id;
        break;
      }
    }

    app_decode(&msg);
  }

  if ((HAL_GetTick() - s_last_tx_ms) >= APP_TX_PERIOD_MS)
  {
    s_last_tx_ms = HAL_GetTick();
    app_transmit();
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
