/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    protocol.c
  * @brief   Extensible binary telemetry packet (TLV) + CRC16 implementation.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "protocol.h"
#include <string.h>

/* USER CODE BEGIN 0 */

/* Golden test vector: fixed input -> fixed frame (incl. CRC).
 * Mirrored 1:1 in tools/packet.py (GOLDEN_* / golden_test).
 *
 * seq = 0x0102, ts = 0x04030201, fields:
 *   MOTOR_A_TEMP = 1000 (100.0 degC)  -> tag 0x01 len 2 value 0x03E8
 *   CTRL_A_TEMP  =  500 ( 50.0 degC)  -> tag 0x05 len 2 value 0x01F4
 *   BAT_SOC      =   80 ( 80 %)       -> tag 0x09 len 1 value 0x50
 */
static const uint8_t PROTO_GOLDEN_FRAME[] =
{
  0xA5U, 0x5AU, 0x02U, 0x12U,                     /* sof, magic, version, length */
  0x02U, 0x01U,                                   /* sequence = 0x0102 (LE)      */
  0x01U, 0x02U, 0x03U, 0x04U,                     /* timestamp = 0x04030201 (LE) */
  0x03U,                                          /* field_count = 3             */
  0x01U, 0x02U, 0xE8U, 0x03U,                     /* MOTOR_A_TEMP = 0x03E8       */
  0x05U, 0x02U, 0xF4U, 0x01U,                     /* CTRL_A_TEMP  = 0x01F4       */
  0x09U, 0x01U, 0x50U,                            /* BAT_SOC      = 0x50         */
  0x24U, 0xFAU                                     /* crc16 = 0xFA24 (LE)         */
};

#define PROTO_GOLDEN_LEN (sizeof(PROTO_GOLDEN_FRAME))

/* USER CODE END 0 */

/**
  * @brief CRC-16/MODBUS (poly 0x8005 reflected = 0xA001, init 0xFFFF).
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

void pkt_begin(pkt_t *p)
{
  uint32_t i;

  p->len = 0U;
  p->field_count = 0U;

  p->data[0] = PKT_SOF;
  p->data[1] = PKT_MAGIC;
  p->data[2] = PKT_VERSION;
  p->data[3] = 0U;               /* length, filled by pkt_end()             */

  /* Reserve and zero the fixed header tail (seq/ts/field_count). */
  for (i = 4U; i < PKT_HDR_SIZE; i++)
  {
    p->data[i] = 0U;
  }

  p->len = PKT_HDR_SIZE;         /* fields start at offset 11               */
}

void pkt_set_seq(pkt_t *p, uint16_t seq)
{
  p->data[4] = (uint8_t)(seq & 0xFFU);
  p->data[5] = (uint8_t)((seq >> 8) & 0xFFU);
}

void pkt_set_ts(pkt_t *p, uint32_t ms)
{
  p->data[6]  = (uint8_t)(ms & 0xFFU);
  p->data[7]  = (uint8_t)((ms >> 8) & 0xFFU);
  p->data[8]  = (uint8_t)((ms >> 16) & 0xFFU);
  p->data[9]  = (uint8_t)((ms >> 24) & 0xFFU);
}

static int pkt_add_raw(pkt_t *p, uint8_t tag, const uint8_t *val, uint8_t val_len)
{
  if ((p == NULL) || (val == NULL) || (val_len == 0U) || (val_len > 4U))
  {
    return -1;
  }

  if (p->field_count >= PKT_MAX_FIELDS)
  {
    return -1;
  }

  if ((p->len + 2U + (uint32_t)val_len + PKT_CRC_SIZE) > PKT_MAX_SIZE)
  {
    return -1;
  }

  p->data[p->len++] = tag;
  p->data[p->len++] = val_len;
  (void)memcpy(&p->data[p->len], val, val_len);
  p->len += val_len;
  p->field_count++;

  return 0;
}

int pkt_add_u8(pkt_t *p, uint8_t tag, uint8_t v)
{
  return pkt_add_raw(p, tag, &v, 1U);
}

int pkt_add_u16(pkt_t *p, uint8_t tag, uint16_t v)
{
  uint8_t b[2] = { (uint8_t)(v & 0xFFU), (uint8_t)((v >> 8) & 0xFFU) };
  return pkt_add_raw(p, tag, b, 2U);
}

int pkt_add_i16(pkt_t *p, uint8_t tag, int16_t v)
{
  return pkt_add_u16(p, tag, (uint16_t)v);
}

int pkt_add_i32(pkt_t *p, uint8_t tag, int32_t v)
{
  return pkt_add_u32(p, tag, (uint32_t)v);
}

int pkt_add_u32(pkt_t *p, uint8_t tag, uint32_t v)
{
  uint8_t b[4] =
  {
    (uint8_t)(v & 0xFFU),
    (uint8_t)((v >> 8) & 0xFFU),
    (uint8_t)((v >> 16) & 0xFFU),
    (uint8_t)((v >> 24) & 0xFFU)
  };
  return pkt_add_raw(p, tag, b, 4U);
}

void pkt_end(pkt_t *p)
{
  uint16_t crc;

  /* payload length = bytes from 'sequence' (offset 4) through last field byte */
  p->data[3]  = (uint8_t)((p->len - 4U) & 0xFFU);
  p->data[10] = (uint8_t)p->field_count;

  /* CRC covers bytes [version .. last field byte] = [2 .. len-1] */
  crc = proto_crc16(&p->data[2], p->len - 2U);

  p->data[p->len++] = (uint8_t)(crc & 0xFFU);
  p->data[p->len++] = (uint8_t)((crc >> 8) & 0xFFU);
}

uint32_t pkt_size(const pkt_t *p)
{
  return p->len;
}

const uint8_t *pkt_data(const pkt_t *p)
{
  return p->data;
}

/**
  * @brief Golden self-test: build a frame from fixed inputs, compare the full
  *        binary image (incl. CRC) against the expected bytes.
  * @retval 1 on pass, 0 on mismatch.
  */
int proto_selftest(void)
{
  pkt_t f;

  pkt_begin(&f);
  pkt_set_seq(&f, 0x0102U);
  pkt_set_ts(&f, 0x04030201UL);
  (void)pkt_add_i16(&f, TAG_MOTOR_A_TEMP, (int16_t)1000);
  (void)pkt_add_i16(&f, TAG_CTRL_A_TEMP,  (int16_t)500);
  (void)pkt_add_u8 (&f, TAG_BAT_SOC,       (uint8_t)80);
  pkt_end(&f);

  if (pkt_size(&f) != PROTO_GOLDEN_LEN)
  {
    return 0;
  }

  return (memcmp(f.data, PROTO_GOLDEN_FRAME, PROTO_GOLDEN_LEN) == 0) ? 1 : 0;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
