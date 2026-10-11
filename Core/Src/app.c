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
/* DBC message IDs (see All.dbc).                                            */
/*   Motor Frame2 (standard 11-bit, little-endian):                          */
/*     byte[3] = ControlerTemp (1, -40) degC                                  */
/*     byte[4] = MotorTemp     (1, -40) degC                                  */
/*   BMS_HCU_INFO (extended 29-bit, Motorola):                                */
/*     byte[4] = BatSoc        (1, 0)   %                                     */
/* -------------------------------------------------------------------------- */
#define CANID_MOTOR_A_FRAME2    0x6A2U   /* 1698 */
#define CANID_MOTOR_B_FRAME2    0x6B2U   /* 1714 */
#define CANID_MOTOR_C_FRAME2    0x6C2U   /* 1730 */
#define CANID_MOTOR_D_FRAME2    0x6D2U   /* 1746 */
#define CANID_MOTOR_A_FRAME1    0x6A1U   /* 1697, FA1 (motor speed)            */
#define CANID_MOTOR_A_FRAME3    0x6A3U   /* 1699, FA3 (bus voltage)            */
#define CANID_CH110_ACCEL       392U     /* 0x188, CH110_TPDO1 (accel X/Y/Z)   */

/* DBC lists 2556444915 (0x986040F3) which exceeds the 29-bit CAN ID range.
 * The value actually seen on the wire is the low 29 bits. If the BMS reports
 * a different ID, adjust this constant (see All.dbc BMS_HCU_INFO). */
#define CANID_BMS_HCU_INFO      0x186040F3U

#define TEMP_OFFSET_DEGC        (-40)     /* DBC factor/offset for temps      */
#define TEMP_SCALE              (10)      /* store 0.1 degC                   */

/* USER CODE END 0 */

/* Private variables ---------------------------------------------------------*/
static uint16_t s_sequence = 0;
static uint32_t s_last_tx_ms = 0;
static uint8_t s_test_counter = 0;

/* Latest decoded values (physical units) + validity flags. */
static int16_t s_motor_temp[4] = {0, 0, 0, 0};
static int16_t s_ctrl_temp[4]  = {0, 0, 0, 0};
static uint8_t s_soc = 0;
static uint8_t s_motor_temp_valid[4] = {0, 0, 0, 0};
static uint8_t s_ctrl_temp_valid[4]  = {0, 0, 0, 0};
static uint8_t s_soc_valid = 0;

/* Acceleration (milli-g), voltages, motor speed. */
static int16_t  s_acc_x = 0, s_acc_y = 0, s_acc_z = 0;
static uint16_t s_bat_voltage = 0;   /* 0.1 V */
static uint16_t s_bus_voltage = 0;   /* V     */
static int16_t  s_motor_speed = 0;   /* rpm   */
static uint8_t  s_acc_valid = 0;
static uint8_t  s_bat_volt_valid = 0;
static uint8_t  s_bus_volt_valid = 0;
static uint8_t  s_speed_valid = 0;

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
  int32_t t;

  if (m->ide == 0U)
  {
    /* Standard 11-bit: motor Frame2 (temperature). */
    uint8_t idx;

    switch (m->id)
    {
      case CANID_MOTOR_A_FRAME2: idx = 0U; break;
      case CANID_MOTOR_B_FRAME2: idx = 1U; break;
      case CANID_MOTOR_C_FRAME2: idx = 2U; break;
      case CANID_MOTOR_D_FRAME2: idx = 3U; break;
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
      case CANID_MOTOR_A_FRAME1:
        /* Motor speed: 16-bit little-endian unsigned, offset -30000 -> rpm. */
        if (m->dlc >= 2U)
        {
          uint16_t raw = (uint16_t)m->data[0] | ((uint16_t)m->data[1] << 8);
          s_motor_speed = (int16_t)((int32_t)raw - 30000);
          s_speed_valid = 1U;
        }
        return;
      case CANID_MOTOR_A_FRAME3:
        /* Bus voltage: 10-bit little-endian unsigned, unit V. */
        if (m->dlc >= 2U)
        {
          s_bus_voltage = (uint16_t)m->data[0] | ((uint16_t)(m->data[1] & 0x03U) << 8);
          s_bus_volt_valid = 1U;
        }
        return;
      default: return;
    }

    if (m->dlc >= 5U)
    {
      t = ((int32_t)m->data[3] + TEMP_OFFSET_DEGC) * TEMP_SCALE;
      s_ctrl_temp[idx] = (int16_t)t;
      s_ctrl_temp_valid[idx] = 1U;

      t = ((int32_t)m->data[4] + TEMP_OFFSET_DEGC) * TEMP_SCALE;
      s_motor_temp[idx] = (int16_t)t;
      s_motor_temp_valid[idx] = 1U;
    }
  }
  else
  {
    /* Extended 29-bit: BMS_HCU_INFO (battery voltage + SOC). */
    if (m->id == CANID_BMS_HCU_INFO)
    {
      if (m->dlc >= 5U)
      {
        /* BatVoltage: 16-bit Motorola @ byte0..1, unit 0.1 V. */
        s_bat_voltage = ((uint16_t)m->data[0] << 8) | (uint16_t)m->data[1];
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

  /* Test counter: increments every packet, used to verify the RF link. */
  (void)pkt_add_u8(&frame, TAG_TEST, s_test_counter++);

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
