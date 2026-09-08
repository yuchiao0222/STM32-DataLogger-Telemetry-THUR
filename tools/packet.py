"""Binary telemetry packet + CRC16 (mirror of the STM32 `protocol.h/.c`).

Frame layout (fixed 46 bytes, little-endian):
    [0]     SOF         0xA5
    [1]     MAGIC       0x5A
    [2]     version     0x01
    [3]     length      payload length (=40)
    [4:6]   sequence    uint16
    [6:10]  timestamp_ms uint32
    [10:24] adc_raw[7]  uint16 x7
    [24]    can_status  uint8 (bit flags)
    [25:33] can1_data[8]
    [33:41] can2_data[8]
    [41]    rf_status   uint8 (bit flags)
    [42:44] reserved
    [44:46] crc16       uint16 (CRC-16/MODBUS over bytes [2:44])
"""

import struct

PKT_SOF = 0xA5
PKT_MAGIC = 0x5A
PKT_VERSION = 0x01
PKT_ADC_CHANNELS = 7
PKT_CAN_DATA_LEN = 8
PKT_PAYLOAD_LEN = 40
PKT_TOTAL_LEN = 46

# CAN status bit flags
CAN_ST_CAN1_RUN = 0x01
CAN_ST_CAN2_RUN = 0x02
CAN_ST_CAN1_PASS = 0x04
CAN_ST_CAN2_PASS = 0x08

# RF status bit flags
RF_ST_AUX = 0x01

# Full frame format (little-endian); crc16 appended separately below.
_FRAME_FMT = "<BBBBHI7HB8B8BB2B"
_CRC_FMT = "<H"


def crc16(data: bytes) -> int:
    """CRC-16/MODBUS (poly 0x8005 reflected = 0xA001, init 0xFFFF).

    Known-answer: crc16(b"123456789") == 0x4B37.
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def pack(sequence: int, timestamp_ms: int, adc_raw, can_status: int,
         can1_data, can2_data, rf_status: int,
         version: int = PKT_VERSION) -> bytes:
    """Build a full frame (with correct CRC)."""
    if len(adc_raw) != PKT_ADC_CHANNELS:
        raise ValueError("adc_raw must have 7 values")
    if len(can1_data) != PKT_CAN_DATA_LEN or len(can2_data) != PKT_CAN_DATA_LEN:
        raise ValueError("can data must have 8 bytes each")

    body = struct.pack(
        _FRAME_FMT,
        PKT_SOF, PKT_MAGIC, version, PKT_PAYLOAD_LEN,
        sequence, timestamp_ms,
        *adc_raw,
        can_status,
        *can1_data,
        *can2_data,
        rf_status,
        0, 0,
    )
    crc = crc16(body[2:])
    return body + struct.pack(_CRC_FMT, crc)


def parse(frame: bytes):
    """Parse and validate a 46-byte frame.

    Returns a dict of fields, or None if length/sync/CRC check fails.
    """
    if len(frame) != PKT_TOTAL_LEN:
        return None
    if frame[0] != PKT_SOF or frame[1] != PKT_MAGIC:
        return None

    crc_received = struct.unpack(_CRC_FMT, frame[-2:])[0]
    crc_calc = crc16(frame[2:-2])
    if crc_received != crc_calc:
        return None

    vals = struct.unpack(_FRAME_FMT, frame[:-2])
    sof, magic, version, length, seq, ts = vals[:6]
    adc = list(vals[6:6 + PKT_ADC_CHANNELS])
    can_status = vals[13]
    can1 = list(vals[14:14 + PKT_CAN_DATA_LEN])
    can2 = list(vals[22:22 + PKT_CAN_DATA_LEN])
    rf_status = vals[30]

    return {
        "sof": sof,
        "magic": magic,
        "version": version,
        "length": length,
        "sequence": seq,
        "timestamp_ms": ts,
        "adc_raw": adc,
        "can_status": can_status,
        "can1_data": can1,
        "can2_data": can2,
        "rf_status": rf_status,
        "crc16": crc_received,
    }


def self_test() -> bool:
    """Validate CRC known-answer + pack/parse round-trip."""
    ok = True

    if crc16(b"123456789") != 0x4B37:
        print("FAIL: CRC known-answer mismatch")
        ok = False

    adc = [1000 + i * 200 for i in range(PKT_ADC_CHANNELS)]
    can1 = [0x11 + i for i in range(PKT_CAN_DATA_LEN)]
    can2 = [0x21 + i for i in range(PKT_CAN_DATA_LEN)]
    frame = pack(42, 123456, adc, 0x0F, can1, can2, RF_ST_AUX)
    parsed = parse(frame)

    if parsed is None:
        print("FAIL: pack/parse round-trip returned None")
        ok = False
    elif parsed["sequence"] != 42 or parsed["adc_raw"] != adc \
            or parsed["can1_data"] != can1 or parsed["can2_data"] != can2:
        print("FAIL: pack/parse round-trip field mismatch")
        ok = False

    # Corrupt one byte -> CRC must fail
    bad = bytearray(frame)
    bad[10] ^= 0xFF
    if parse(bytes(bad)) is not None:
        print("FAIL: corrupted frame passed CRC")
        ok = False

    print("self_test:", "PASS" if ok else "FAIL")
    return ok


if __name__ == "__main__":
    raise SystemExit(0 if self_test() else 1)
