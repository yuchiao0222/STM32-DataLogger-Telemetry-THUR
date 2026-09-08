/*
 * Host-side golden test for protocol.c (proves STM32 C == Python packet.py).
 *
 * Build (any C99 host compiler):
 *     gcc -I ../Core/Inc host_proto_test.c ../Core/Src/protocol.c -o host_proto_test
 * Run:
 *     ./host_proto_test
 *
 * It builds the same fixed golden frame as tools/packet.py GOLDEN_FRAME,
 * prints its hex dump, then runs proto_selftest() (which compares against the
 * hard-coded expected bytes). If the printed hex matches the Python golden
 * hex, the two implementations are byte-identical.
 */

#include <stdio.h>
#include <string.h>
#include "protocol.h"

int main(void)
{
  static const uint16_t adc[PKT_ADC_CHANNELS] =
    {0x0100U, 0x0201U, 0x0302U, 0x0403U, 0x0504U, 0x0605U, 0x0706U};
  static const uint8_t can1[PKT_CAN_DATA_LEN] =
    {0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U};
  static const uint8_t can2[PKT_CAN_DATA_LEN] =
    {0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U};

  pkt_frame_t f;
  const uint8_t *p;
  uint32_t i;
  uint8_t k;

  proto_packet_init(&f);
  f.sequence = 0x0102U;
  f.timestamp_ms = 0x04030201UL;
  for (k = 0U; k < PKT_ADC_CHANNELS; k++) { f.adc_raw[k] = adc[k]; }
  f.can_status = 0x0FU;
  for (k = 0U; k < PKT_CAN_DATA_LEN; k++)
  {
    f.can1_data[k] = can1[k];
    f.can2_data[k] = can2[k];
  }
  f.rf_status = 0x01U;
  proto_packet_finalize(&f);

  p = (const uint8_t *)&f;
  printf("frame_size=%u\n", (unsigned)proto_packet_size());
  printf("hex=");
  for (i = 0U; i < proto_packet_size(); i++)
  {
    printf("%02X%s", p[i], (i + 1U < proto_packet_size()) ? " " : "\n");
  }

  printf("crc16(\"123456789\")=0x%04X\n", proto_crc16((const uint8_t *)"123456789", 9U));
  printf("selftest=%s\n", proto_selftest() ? "PASS" : "FAIL");

  return proto_selftest() ? 0 : 1;
}
