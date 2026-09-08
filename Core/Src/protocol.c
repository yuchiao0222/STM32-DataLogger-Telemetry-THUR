/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    protocol.c
  * @brief   Binary telemetry packet + CRC16 implementation.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "protocol.h"
#include <string.h>

/* USER CODE BEGIN 0 */

/* Golden test vector: fixed input -> fixed 46-byte frame (incl. CRC).
 * Mirrored 1:1 in tools/packet.py (GOLDEN_FRAME / golden_test). */
static const uint8_t PROTO_GOLDEN_FRAME[PKT_TOTAL_LEN] =
{
  0xA5U, 0x5AU, 0x01U, 0x28U,                     /* sof, magic, version, length */
  0x02U, 0x01U,                                   /* sequence = 0x0102 (LE)      */
  0x01U, 0x02U, 0x03U, 0x04U,                     /* timestamp = 0x04030201 (LE) */
  0x00U, 0x01U, 0x01U, 0x02U, 0x02U, 0x03U,       /* adc_raw[7] (LE)             */
  0x03U, 0x04U, 0x04U, 0x05U, 0x05U, 0x06U,
  0x06U, 0x07U,
  0x0FU,                                          /* can_status                  */
  0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U, /* can1_data[8]   */
  0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U, /* can2_data[8]   */
  0x01U,                                          /* rf_status                   */
  0x00U, 0x00U,                                   /* reserved                    */
  0x68U, 0xDAU                                    /* crc16 = 0xDA68 (LE)         */
};

/* USER CODE END 0 */

/**
  * @brief CRC-16/MODBUS (poly 0x8005 reflected = 0xA001, init 0xFFFF).
  * @param data  Input byte array
  * @param len   Number of bytes
  * @retval 16-bit CRC value
  */
uint16_t proto_crc16(const uint8_t *data, uint32_t len)
{
  uint16_t crc = 0xFFFFU;
  uint32_t i;

  for (i = 0U; i < len; i++)
  {
    uint8_t j;

    crc ^= (uint16_t)data[i];

    for (j = 0U; j < 8U; j++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc = (uint16_t)((crc >> 1) ^ 0xA001U);
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

/**
  * @brief Initialize framing fields.
  */
void proto_packet_init(pkt_frame_t *f)
{
  f->sof = PKT_SOF;
  f->magic = PKT_MAGIC;
  f->version = PKT_VERSION;
  f->length = PKT_PAYLOAD_LEN;
  f->reserved[0] = 0U;
  f->reserved[1] = 0U;
}

/**
  * @brief Compute CRC16 over version..reserved and store into crc16 field.
  */
void proto_packet_finalize(pkt_frame_t *f)
{
  uint32_t off = (uint32_t)offsetof(pkt_frame_t, version);
  uint32_t len = (uint32_t)(offsetof(pkt_frame_t, crc16) - offsetof(pkt_frame_t, version));

  f->crc16 = proto_crc16(((const uint8_t *)f) + off, len);
}

/**
  * @brief Total frame size.
  */
uint32_t proto_packet_size(void)
{
  return (uint32_t)sizeof(pkt_frame_t);
}

/**
  * @brief Golden self-test: build a frame from fixed inputs and compare the
  *        full binary image (incl. CRC) against the expected bytes.
  * @retval 1 on pass, 0 on mismatch.
  */
int proto_selftest(void)
{
  static const uint16_t adc[PKT_ADC_CHANNELS] =
    {0x0100U, 0x0201U, 0x0302U, 0x0403U, 0x0504U, 0x0605U, 0x0706U};
  static const uint8_t can1[PKT_CAN_DATA_LEN] =
    {0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U};
  static const uint8_t can2[PKT_CAN_DATA_LEN] =
    {0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U};

  pkt_frame_t f;
  uint8_t i;

  proto_packet_init(&f);
  f.sequence = 0x0102U;
  f.timestamp_ms = 0x04030201UL;

  for (i = 0U; i < PKT_ADC_CHANNELS; i++)
  {
    f.adc_raw[i] = adc[i];
  }
  f.can_status = 0x0FU;
  for (i = 0U; i < PKT_CAN_DATA_LEN; i++)
  {
    f.can1_data[i] = can1[i];
    f.can2_data[i] = can2[i];
  }
  f.rf_status = 0x01U;

  proto_packet_finalize(&f);

  return (memcmp(&f, PROTO_GOLDEN_FRAME, PKT_TOTAL_LEN) == 0) ? 1 : 0;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
