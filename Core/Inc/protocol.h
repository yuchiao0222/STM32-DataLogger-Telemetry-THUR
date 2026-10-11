/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    protocol.h
  * @brief   Extensible binary telemetry protocol (TLV) + CRC16.
  *
  *          Frame layout (little-endian):
  *            [0]      SOF          0xA5
  *            [1]      MAGIC        0x5A
  *            [2]      version      0x02
  *            [3]      length       payload byte count (offset 4 .. crc-1)
  *            [4:6]    sequence     uint16
  *            [6:10]   timestamp_ms uint32
  *            [10]     field_count  uint8
  *            [11..]   fields       each = [tag:1][len:1][value:len]
  *            [..]     crc16        uint16 (CRC-16/MODBUS over bytes [2..crc-1])
  *
  *          The field list is a TLV stream: adding or removing a measurement
  *          only adds/removes one field. A receiver skips any tag it does not
  *          recognize, so the two ends stay compatible across schema changes.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

/* Packet framing ------------------------------------------------------------*/
#define PKT_SOF             0xA5U
#define PKT_MAGIC           0x5AU
#define PKT_VERSION         0x02U

/* Fixed header size: SOF + MAGIC + version + length + seq(2) + ts(4) + count(1) */
#define PKT_HDR_SIZE        11U
#define PKT_CRC_SIZE        2U
#define PKT_MAX_FIELDS      32U
#define PKT_MAX_FIELD_SIZE  6U   /* tag(1) + len(1) + value(<=4)              */
#define PKT_MAX_SIZE        (PKT_HDR_SIZE + (PKT_MAX_FIELDS * PKT_MAX_FIELD_SIZE) + PKT_CRC_SIZE)

/* Field tags -----------------------------------------------------------------
 * Each tag identifies one physical measurement. To add a new signal, define a
 * new tag here and mirror it in tools/packet.py (keep the two lists in sync). */
#define TAG_MOTOR_A_TEMP    0x01U   /* int16, 0.1 degC, DBC FA2_MotorTemp      */
#define TAG_MOTOR_B_TEMP    0x02U   /* int16, 0.1 degC, DBC FB2_MotorTemp      */
#define TAG_MOTOR_C_TEMP    0x03U   /* int16, 0.1 degC, DBC FC2_MotorTemp      */
#define TAG_MOTOR_D_TEMP    0x04U   /* int16, 0.1 degC, DBC FD2_MotorTemp      */
#define TAG_CTRL_A_TEMP     0x05U   /* int16, 0.1 degC, DBC FA2_ControlerTemp  */
#define TAG_CTRL_B_TEMP     0x06U   /* int16, 0.1 degC, DBC FB2_ControlerTemp  */
#define TAG_CTRL_C_TEMP     0x07U   /* int16, 0.1 degC, DBC FC2_ControlerTemp  */
#define TAG_CTRL_D_TEMP     0x08U   /* int16, 0.1 degC, DBC FD2_ControlerTemp  */
#define TAG_BAT_SOC         0x09U   /* uint8, %, DBC BatSoc (BMS_HCU_INFO)     */
#define TAG_ACC_X           0x0AU   /* int16, milli-g, DBC Acc_X (CH110)       */
#define TAG_ACC_Y           0x0BU   /* int16, milli-g, DBC Acc_Y (CH110)       */
#define TAG_ACC_Z           0x0CU   /* int16, milli-g, DBC Acc_Z (CH110)       */
#define TAG_BAT_VOLTAGE     0x0DU   /* uint16, 0.1 V, DBC BatVoltage           */
#define TAG_BUS_VOLTAGE     0x0EU   /* uint16, V, DBC FA3_BusVoltage           */
#define TAG_MOTOR_SPEED     0x0FU   /* int16, rpm, DBC FA1_Motor_speed         */
#define TAG_TEST            0x20U   /* uint8, test counter, increments each pkt */

/* Packet builder -------------------------------------------------------------*/
typedef struct
{
  uint8_t  data[PKT_MAX_SIZE];
  uint32_t len;         /* bytes written so far                              */
  uint16_t field_count;
} pkt_t;

/* Start a new frame (SOF/MAGIC/version, zero length + field count). */
void pkt_begin(pkt_t *p);

/* Set the sequence number (uint16, LE). */
void pkt_set_seq(pkt_t *p, uint16_t seq);

/* Set the timestamp in milliseconds (uint32, LE). */
void pkt_set_ts(pkt_t *p, uint32_t ms);

/* Append one field. @retval 0 on success, -1 if the buffer would overflow. */
int pkt_add_u8 (pkt_t *p, uint8_t tag, uint8_t  v);
int pkt_add_u16(pkt_t *p, uint8_t tag, uint16_t v);
int pkt_add_i16(pkt_t *p, uint8_t tag, int16_t  v);
int pkt_add_i32(pkt_t *p, uint8_t tag, int32_t  v);
int pkt_add_u32(pkt_t *p, uint8_t tag, uint32_t v);

/* Finalize: store 'length' and 'field_count', append CRC16. Must be called
 * once after all fields are added and before pkt_size()/pkt_data(). */
void pkt_end(pkt_t *p);

/* Total frame size in bytes (valid only after pkt_end()). */
uint32_t pkt_size(const pkt_t *p);

/* Pointer to the frame bytes (valid only after pkt_end()). */
const uint8_t *pkt_data(const pkt_t *p);

/* CRC-16/MODBUS: poly 0x8005 (reflected 0xA001), init 0xFFFF.
 * Known-answer check: proto_crc16("123456789", 9) == 0x4B37. */
uint16_t proto_crc16(const uint8_t *data, uint32_t len);

/* Self-test: build a golden frame from a fixed input vector and compare the
 * full binary image (including CRC) against the expected bytes mirrored in
 * tools/packet.py. @retval 1 on pass, 0 on mismatch. */
int proto_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H__ */
