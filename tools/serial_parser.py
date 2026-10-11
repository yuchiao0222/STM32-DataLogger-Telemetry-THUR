#!/usr/bin/env python3
"""PC-side telemetry receiver / parser for the AEBS_RF_Test data link.

Reads the extensible TLV packet stream from a serial port (or a raw log file),
syncs on SOF+MAGIC, validates CRC-16/MODBUS, prints each packet and appends it
to a CSV file. Unknown field tags are skipped, so the receiver stays compatible
as fields are added/removed on the firmware side.

Usage:
    python serial_parser.py --port COM3 --baud 9600 --csv out.csv
    python serial_parser.py --file raw.bin --csv out.csv
    python serial_parser.py --selftest

Dependencies:
    pyserial  (only required for --port; install with: pip install pyserial)
"""

import argparse
import csv
import sys

import packet

# Fixed set of columns for CSV (subset of the known field names).
CSV_HEADER = [
    "sequence", "timestamp_ms",
    "motor_a_temp", "motor_b_temp", "motor_c_temp", "motor_d_temp",
    "ctrl_a_temp", "ctrl_b_temp", "ctrl_c_temp", "ctrl_d_temp",
    "bat_soc",
    "acc_x", "acc_y", "acc_z",
    "bat_voltage", "bus_voltage",
    "motor_speed",
    "test_counter",
    "crc_ok",
]


class FrameExtractor:
    """Extract variable-size frames from a byte stream, resyncing on SOF+MAGIC."""

    def __init__(self):
        self._buf = bytearray()

    def feed(self, chunk: bytes):
        self._buf += chunk
        while True:
            idx = self._buf.find(bytes([packet.PKT_SOF, packet.PKT_MAGIC]))
            if idx < 0:
                self._buf = self._buf[-1:]
                return
            if idx > 0:
                del self._buf[:idx]
            if len(self._buf) < 6:
                return
            # length is at offset 3; total = 4 + length + 2 (crc)
            total = 4 + self._buf[3] + 2
            if len(self._buf) < total:
                return
            frame = bytes(self._buf[:total])
            parsed = packet.parse(frame)
            if parsed is not None:
                del self._buf[:total]
                yield parsed
            else:
                del self._buf[:1]


def print_packet(p):
    dec = packet.decode_fields(p["fields"])
    row = " ".join("%s=%.1f" % (k, v) for k, v in sorted(dec.items()))
    print("seq=%5d ts=%8d ms  %s" % (p["sequence"], p["timestamp_ms"], row))


def row_of(p):
    dec = packet.decode_fields(p["fields"])
    return [
        p["sequence"], p["timestamp_ms"],
        dec.get("motor_a_temp", ""), dec.get("motor_b_temp", ""),
        dec.get("motor_c_temp", ""), dec.get("motor_d_temp", ""),
        dec.get("ctrl_a_temp", ""), dec.get("ctrl_b_temp", ""),
        dec.get("ctrl_c_temp", ""), dec.get("ctrl_d_temp", ""),
        dec.get("bat_soc", ""),
        dec.get("acc_x", ""), dec.get("acc_y", ""), dec.get("acc_z", ""),
        dec.get("bat_voltage", ""), dec.get("bus_voltage", ""),
        dec.get("motor_speed", ""),
        dec.get("test_counter", ""),
        1,
    ]


def run_stream(reader, csv_writer, stats):
    extractor = FrameExtractor()
    for chunk in reader:
        for p in extractor.feed(chunk):
            stats["valid"] += 1
            print_packet(p)
            if csv_writer is not None:
                csv_writer.writerow(row_of(p))


def iter_serial(port, baud):
    try:
        import serial  # pyserial
    except ImportError:
        sys.exit("pyserial is not installed. Run: pip install pyserial")

    ser = serial.Serial(port, baud, timeout=1)
    print("Opened %s @ %d baud" % (port, baud))

    def reader():
        while True:
            data = ser.read(1024)
            if not data:
                continue
            yield data

    return reader()


def iter_file(path):
    f = open(path, "rb")

    def reader():
        while True:
            data = f.read(1024)
            if not data:
                return
            yield data

    return reader()


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port name, e.g. COM3")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--file", help="raw binary log file instead of serial")
    ap.add_argument("--csv", help="output CSV path")
    ap.add_argument("--selftest", action="store_true",
                    help="run CRC/pack self-test and exit")
    args = ap.parse_args(argv)

    if args.selftest:
        raise SystemExit(0 if packet.self_test() else 1)

    if not args.port and not args.file:
        ap.error("provide --port or --file (or --selftest)")

    reader = iter_file(args.file) if args.file else iter_serial(args.port, args.baud)

    csv_file = None
    csv_writer = None
    if args.csv:
        csv_file = open(args.csv, "w", newline="")
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(CSV_HEADER)

    stats = {"valid": 0}
    try:
        run_stream(reader, csv_writer, stats)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        if csv_file is not None:
            csv_file.close()
        print("valid packets: %d" % stats["valid"])


if __name__ == "__main__":
    main(sys.argv[1:])
