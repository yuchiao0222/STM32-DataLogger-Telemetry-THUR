/*
 * Host-side golden test for protocol.c (proves STM32 C == Python packet.py).
 *
 * Build (any C99 host compiler):
 *     gcc -I ../Core/Inc host_proto_test.c ../Core/Src/protocol.c -o host_proto_test
 * Run:
 *     ./host_proto_test
 *
 * It builds the same fixed golden frame as tools/packet.py GOLDEN_FRAME, prints
 * its hex dump, then runs proto_selftest(). If the printed hex matches the
 * Python golden hex, the two implementations are byte-identical.
 */

#include <stdio.h>
#include <string.h>
#include "protocol.h"

int main(void)
{
  pkt_t f;
  const uint8_t *p;
  uint32_t i;

  pkt_begin(&f);
  pkt_set_seq(&f, 0x0102U);
  pkt_set_ts(&f, 0x04030201UL);
  (void)pkt_add_i16(&f, TAG_MOTOR_A_TEMP, (int16_t)1000);
  (void)pkt_add_i16(&f, TAG_CTRL_A_TEMP,  (int16_t)500);
  (void)pkt_add_u8 (&f, TAG_BAT_SOC,       (uint8_t)80);
  pkt_end(&f);

  p = pkt_data(&f);
  printf("frame_size=%u\n", (unsigned)pkt_size(&f));
  printf("hex=");
  for (i = 0U; i < pkt_size(&f); i++)
  {
    printf("%02X%s", p[i], (i + 1U < pkt_size(&f)) ? " " : "\n");
  }

  printf("crc16(\"123456789\")=0x%04X\n", proto_crc16((const uint8_t *)"123456789", 9U));
  printf("selftest=%s\n", proto_selftest() ? "PASS" : "FAIL");

  return proto_selftest() ? 0 : 1;
}
