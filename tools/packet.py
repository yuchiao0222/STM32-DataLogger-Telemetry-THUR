"""Extensible binary telemetry packet (TLV) + CRC16 (mirror of protocol.h/.c).

Frame layout (little-endian):
    [0]     SOF         0xA5
    [1]     MAGIC       0x5A
    [2]     version     0x02
    [3]     length      payload byte count (offset 4 .. crc-1)
    [4:6]   sequence    uint16
    [6:10]  timestamp_ms uint32
    [10]    field_count uint8
    [11..]  fields      each = [tag:1][len:1][value:len]
    [..]    crc16       uint16 (CRC-16/MODBUS over bytes [2..crc-1])

Adding/removing a measurement = adding/removing one field. A decoder skips
unknown tags, so both ends stay compatible across schema changes.
"""

import struct

PKT_SOF = 0xA5
PKT_MAGIC = 0x5A
PKT_VERSION = 0x02

# Field tags (keep in sync with Core/Inc/protocol.h).
TAG_MOTOR_A_TEMP = 0x01  # int16, 0.1 degC
TAG_MOTOR_B_TEMP = 0x02
TAG_MOTOR_C_TEMP = 0x03
TAG_MOTOR_D_TEMP = 0x04
TAG_CTRL_A_TEMP = 0x05   # int16, 0.1 degC
TAG_CTRL_B_TEMP = 0x06
TAG_CTRL_C_TEMP = 0x07
TAG_CTRL_D_TEMP = 0x08
TAG_BAT_SOC = 0x09       # uint8, %

# tag -> (name, decoder). decoder takes raw bytes and returns a physical value.
FIELD_DECODERS = {
    TAG_MOTOR_A_TEMP: ("motor_a_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_MOTOR_B_TEMP: ("motor_b_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_MOTOR_C_TEMP: ("motor_c_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_MOTOR_D_TEMP: ("motor_d_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_CTRL_A_TEMP: ("ctrl_a_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_CTRL_B_TEMP: ("ctrl_b_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_CTRL_C_TEMP: ("ctrl_c_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_CTRL_D_TEMP: ("ctrl_d_temp", lambda b: struct.unpack("<h", b)[0] / 10.0),
    TAG_BAT_SOC: ("bat_soc", lambda b: b[0]),
}

# Golden test vector (mirrors protocol.c PROTO_GOLDEN_FRAME).
GOLDEN_SEQUENCE = 0x0102
GOLDEN_TIMESTAMP = 0x04030201
GOLDEN_FIELDS = [
    (TAG_MOTOR_A_TEMP, struct.pack("<h", 1000)),
    (TAG_CTRL_A_TEMP, struct.pack("<h", 500)),
    (TAG_BAT_SOC, bytes([80])),
]
GOLDEN_FRAME = bytes([
    0xA5, 0x5A, 0x02, 0x12,
    0x02, 0x01,
    0x01, 0x02, 0x03, 0x04,
    0x03,
    0x01, 0x02, 0xE8, 0x03,
    0x05, 0x02, 0xF4, 0x01,
    0x09, 0x01, 0x50,
    0x24, 0xFA,
])


def crc16(data: bytes) -> int:
    """CRC-16/MODBUS (poly 0x8005 reflected = 0xA001, init 0xFFFF)."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


# Typed value encoders (little-endian).
def u8(v: int) -> bytes:
    return bytes([v & 0xFF])


def i16(v: int) -> bytes:
    return struct.pack("<h", v)


def u16(v: int) -> bytes:
    return struct.pack("<H", v)


def i32(v: int) -> bytes:
    return struct.pack("<i", v)


def u32(v: int) -> bytes:
    return struct.pack("<I", v)


def pack(sequence: int, timestamp_ms: int, fields) -> bytes:
    """Build a full frame from a list of (tag, value_bytes) tuples."""
    body = b"".join(bytes([tag, len(val)]) + val for tag, val in fields)
    length = 2 + 4 + 1 + len(body)
    inner = (bytes([PKT_VERSION, length])
             + struct.pack("<H", sequence)
             + struct.pack("<I", timestamp_ms)
             + bytes([len(fields)])
             + body)
    crc = crc16(inner)
    return bytes([PKT_SOF, PKT_MAGIC]) + inner + struct.pack("<H", crc)


def parse(frame: bytes):
    """Parse and validate a frame. Returns a dict of fields, or None on error.

    The returned dict has: version, length, sequence, timestamp_ms, and
    'fields' = {tag: raw_value_bytes}. Unknown tags are kept raw so a caller
    can decode what it recognizes and ignore the rest.
    """
    if len(frame) < 11:
        return None
    if frame[0] != PKT_SOF or frame[1] != PKT_MAGIC:
        return None

    length = frame[3]
    if len(frame) != 4 + length + 2:
        return None

    crc_rx = struct.unpack("<H", frame[-2:])[0]
    if crc_rx != crc16(frame[2:-2]):
        return None

    version = frame[2]
    sequence = struct.unpack("<H", frame[4:6])[0]
    timestamp_ms = struct.unpack("<I", frame[6:10])[0]
    count = frame[10]

    fields = {}
    i = 11
    try:
        for _ in range(count):
            tag = frame[i]
            ln = frame[i + 1]
            fields[tag] = frame[i + 2:i + 2 + ln]
            i += 2 + ln
    except IndexError:
        return None

    return {
        "version": version,
        "length": length,
        "sequence": sequence,
        "timestamp_ms": timestamp_ms,
        "fields": fields,
    }


def decode_fields(fields) -> dict:
    """Decode raw fields into a dict of name -> physical value.

    Unknown tags are skipped, so new fields added by the firmware do not break
    an older receiver.
    """
    out = {}
    for tag, raw in fields.items():
        dec = FIELD_DECODERS.get(tag)
        if dec is not None:
            out[dec[0]] = dec[1](raw)
    return out


def mock_fields(sequence: int = 0):
    """A plausible set of fields for offline tests (matches app.c tags)."""
    base = sequence % 100
    fields = []
    for i in range(4):
        fields.append((TAG_MOTOR_A_TEMP + i, i16((500 + i * 100 + base) & 0xFFFF)))
        fields.append((TAG_CTRL_A_TEMP + i, i16((400 + i * 50 + base) & 0xFFFF)))
    fields.append((TAG_BAT_SOC, u8(80 + (base % 20))))
    return fields


def mock_frame(sequence: int, timestamp_ms: int) -> bytes:
    return pack(sequence, timestamp_ms, mock_fields(sequence))


def golden_test() -> bool:
    return pack(GOLDEN_SEQUENCE, GOLDEN_TIMESTAMP, GOLDEN_FIELDS) == GOLDEN_FRAME


def self_test() -> bool:
    ok = True

    if crc16(b"123456789") != 0x4B37:
        print("FAIL: CRC known-answer mismatch")
        ok = False

    if not golden_test():
        print("FAIL: golden vector mismatch")
        ok = False

    # pack/parse round-trip
    frame = mock_frame(42, 123456)
    parsed = parse(frame)
    if parsed is None:
        print("FAIL: round-trip returned None")
        ok = False
    else:
        if parsed["sequence"] != 42 or parsed["timestamp_ms"] != 123456:
            print("FAIL: round-trip header mismatch")
            ok = False
        decoded = decode_fields(parsed["fields"])
        if "motor_a_temp" not in decoded or "bat_soc" not in decoded:
            print("FAIL: round-trip fields missing")
            ok = False

    # corrupted byte must fail CRC
    bad = bytearray(frame)
    bad[10] ^= 0xFF
    if parse(bytes(bad)) is not None:
        print("FAIL: corrupted frame passed CRC")
        ok = False

    # unknown tag must be ignored, not break parsing
    frame2 = pack(7, 1, [(TAG_MOTOR_A_TEMP, i16(100)), (0xEE, b"\x01\x02\x03")])
    parsed2 = parse(frame2)
    if parsed2 is None:
        print("FAIL: unknown tag broke parsing")
        ok = False
    else:
        dec2 = decode_fields(parsed2["fields"])
        if "motor_a_temp" not in dec2:
            print("FAIL: known tag lost alongside unknown tag")
            ok = False

    print("self_test:", "PASS" if ok else "FAIL")
    return ok


if __name__ == "__main__":
    raise SystemExit(0 if self_test() else 1)
