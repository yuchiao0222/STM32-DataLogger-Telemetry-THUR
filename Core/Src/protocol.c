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

/* USER CODE BEGIN 0 */

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

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
