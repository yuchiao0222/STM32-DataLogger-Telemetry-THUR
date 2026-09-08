/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    protocol.h
  * @brief   Binary telemetry packet definition + CRC16 for the
  *          data-acquisition / data-transmission application layer.
  *
  *          Frame layout (fixed 46 bytes, little-endian):
  *            [0]  SOF        0xA5
  *            [1]  MAGIC      0x5A
  *            [2]  version    0x01
  *            [3]  length     payload length (=40)
  *            [4:5]  sequence uint16
  *            [6:9]  timestamp_ms uint32
  *            [10:23] adc_raw[7] uint16 x7
  *            [24]   can_status uint8 (bit flags)
  *            [25:32] can1_data[8]
  *            [33:40] can2_data[8]
  *            [41]   rf_status uint8 (bit flags)
  *            [42:43] reserved
  *            [44:45] crc16 uint16 (CRC-16/MODBUS over bytes [2:44])
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
#define PKT_VERSION         0x01U

/* Field sizes ---------------------------------------------------------------*/
#define PKT_ADC_CHANNELS    7U
#define PKT_CAN_DATA_LEN    8U

/* Packet sizes --------------------------------------------------------------*/
#define PKT_PAYLOAD_LEN     40U   /* bytes between 'length' field and crc16   */
#define PKT_TOTAL_LEN       46U   /* full frame                                */

/* CAN status bit flags ------------------------------------------------------*/
#define CAN_ST_CAN1_RUN     0x01U /* CAN1 started (listening)                 */
#define CAN_ST_CAN2_RUN     0x02U /* CAN2 started (listening)                 */
#define CAN_ST_CAN1_PASS    0x04U /* CAN1 loopback last sample PASS           */
#define CAN_ST_CAN2_PASS    0x08U /* CAN2 loopback last sample PASS           */

/* RF status bit flags -------------------------------------------------------*/
#define RF_ST_AUX           0x01U /* RF AUX pin state                          */

/**
  * @brief Packed binary frame. CRC16 covers bytes [version .. reserved[1]]
  *        i.e. offsetof(version) .. offsetof(crc16)-1.
  */
typedef struct __attribute__((packed))
{
  uint8_t  sof;
  uint8_t  magic;
  uint8_t  version;
  uint8_t  length;
  uint16_t sequence;
  uint32_t timestamp_ms;
  uint16_t adc_raw[PKT_ADC_CHANNELS];
  uint8_t  can_status;
  uint8_t  can1_data[PKT_CAN_DATA_LEN];
  uint8_t  can2_data[PKT_CAN_DATA_LEN];
  uint8_t  rf_status;
  uint8_t  reserved[2];
  uint16_t crc16;
} pkt_frame_t;

/* Compile-time layout guards. These lock the packed frame layout so that any
 * accidental field reordering / padding / endianness change breaks the build
 * (the packet must remain byte-identical with tools/packet.py). */
typedef char proto_assert_frame_size[(sizeof(pkt_frame_t) == PKT_TOTAL_LEN) ? 1 : -1];
typedef char proto_assert_off_sequence[(offsetof(pkt_frame_t, sequence) == 4) ? 1 : -1];
typedef char proto_assert_off_timestamp[(offsetof(pkt_frame_t, timestamp_ms) == 6) ? 1 : -1];
typedef char proto_assert_off_adc[(offsetof(pkt_frame_t, adc_raw) == 10) ? 1 : -1];
typedef char proto_assert_off_can_status[(offsetof(pkt_frame_t, can_status) == 24) ? 1 : -1];
typedef char proto_assert_off_can1[(offsetof(pkt_frame_t, can1_data) == 25) ? 1 : -1];
typedef char proto_assert_off_can2[(offsetof(pkt_frame_t, can2_data) == 33) ? 1 : -1];
typedef char proto_assert_off_rf[(offsetof(pkt_frame_t, rf_status) == 41) ? 1 : -1];
typedef char proto_assert_off_crc[(offsetof(pkt_frame_t, crc16) == 44) ? 1 : -1];

/* CRC-16/MODBUS: poly 0x8005 (reflected 0xA001), init 0xFFFF.
 * Known-answer check: proto_crc16("123456789", 9) == 0x4B37. */
uint16_t proto_crc16(const uint8_t *data, uint32_t len);

/* Initialize framing fields (SOF/MAGIC/version/length/reserved). */
void proto_packet_init(pkt_frame_t *f);

/* Compute and store crc16 over the covered region. */
void proto_packet_finalize(pkt_frame_t *f);

/* Return the total frame size in bytes. */
uint32_t proto_packet_size(void);

/* Self-test: build a golden frame from a fixed input vector and compare the
 * full 46-byte binary image (including CRC) against the expected bytes.
 * The same golden vector is mirrored in tools/packet.py (golden_test) so the
 * STM32 C and the Python implementations are proven byte-identical.
 * @retval 1 on pass, 0 on mismatch. */
int proto_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H__ */
