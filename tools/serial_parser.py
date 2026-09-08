#!/usr/bin/env python3
"""PC-side telemetry receiver / parser for the AEBS_RF_Test data link.

Reads the binary packet stream from a CH343 USB-serial port (or a raw log
file), syncs on SOF+MAGIC, validates CRC-16/MODBUS, prints each packet and
appends it to a CSV file.

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
import time

import packet

CSV_HEADER = [
    "sequence", "timestamp_ms",
    "adc0", "adc1", "adc2", "adc3", "adc4", "adc5", "adc6",
    "can_status", "can1_data", "can2_data", "rf_status",
    "crc_ok",
]


class FrameExtractor:
    """Extract fixed-size frames from a byte stream, resyncing on SOF+MAGIC."""

    def __init__(self):
        self._buf = bytearray()

    def feed(self, chunk: bytes):
        self._buf += chunk
        while True:
            idx = self._buf.find(bytes([packet.PKT_SOF, packet.PKT_MAGIC]))
            if idx < 0:
                # No sync found; keep at most the last byte (could be a SOF).
                self._buf = self._buf[-1:]
                return
            if idx > 0:
                del self._buf[:idx]
            if len(self._buf) < packet.PKT_TOTAL_LEN:
                return
            frame = bytes(self._buf[:packet.PKT_TOTAL_LEN])
            # Validate sync + parse; a valid frame advances a full frame,
            # otherwise advance one byte to resync.
            parsed = packet.parse(frame)
            if parsed is not None:
                del self._buf[:packet.PKT_TOTAL_LEN]
                yield parsed
            else:
                del self._buf[:1]


def format_can(data):
    return " ".join("%02X" % b for b in data)


def print_packet(p):
    print(
        "seq=%5d ts=%8d ms  "
        "ADC=[%5d %5d %5d %5d %5d %5d %5d]  "
        "CAN_st=0x%02X  CAN1=[%s]  CAN2=[%s]  RF=0x%02X"
        % (
            p["sequence"], p["timestamp_ms"],
            p["adc_raw"][0], p["adc_raw"][1], p["adc_raw"][2],
            p["adc_raw"][3], p["adc_raw"][4], p["adc_raw"][5],
            p["adc_raw"][6],
            p["can_status"],
            format_can(p["can1_data"]), format_can(p["can2_data"]),
            p["rf_status"],
        )
    )


def row_of(p):
    return [
        p["sequence"], p["timestamp_ms"],
        *p["adc_raw"],
        p["can_status"],
        " ".join("%02X" % b for b in p["can1_data"]),
        " ".join("%02X" % b for b in p["can2_data"]),
        p["rf_status"],
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
