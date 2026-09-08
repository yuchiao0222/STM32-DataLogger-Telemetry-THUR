#!/usr/bin/env python3
"""Offline (no-hardware) robustness tests for the telemetry link.

Exercises the PC-side pipeline (packet.py + serial_parser.FrameExtractor)
against:
  * continuous valid packets
  * packets with corrupted CRC
  * truncated packets / split-across-chunks framing
  * noise injection followed by re-synchronization

Run:  python test_offline.py        (from this directory)
"""

import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import packet
import serial_parser


def _drain(extractor, chunks):
    """Feed chunks into a fresh extractor, return the list of parsed dicts."""
    out = []
    for chunk in chunks:
        out.extend(extractor.feed(chunk))
    return out


def _frames(count, start_seq=0, ts_step=1000):
    """Build `count` valid mock frames (mirrors STM32 app.c mock mode)."""
    return [packet.mock_frame(start_seq + i, i * ts_step) for i in range(count)]


def test_continuous():
    n = 50
    frames = _frames(n)
    stream = b"".join(frames)
    out = _drain(serial_parser.FrameExtractor(), [stream])
    assert len(out) == n, "expected %d, got %d" % (n, len(out))
    assert [p["sequence"] for p in out] == list(range(n)), "sequence mismatch"
    for i, p in enumerate(out):
        assert p["adc_raw"] == [(1000 + c * 200 + (i % 100)) & 0xFFFF
                                for c in range(packet.PKT_ADC_CHANNELS)]
    return n


def test_bad_crc():
    n = 10
    frames = _frames(n)
    bad_idx = 5
    corrupted = bytearray(frames[bad_idx])
    corrupted[30] ^= 0xFF
    frames[bad_idx] = bytes(corrupted)
    stream = b"".join(frames)
    out = _drain(serial_parser.FrameExtractor(), [stream])
    seqs = [p["sequence"] for p in out]
    assert bad_idx not in seqs, "corrupted frame was accepted"
    assert len(out) == n - 1, "expected %d valid, got %d" % (n - 1, len(out))
    return n - 1


def test_truncated():
    n = 20
    frames = _frames(n)
    stream = b"".join(frames)

    # (a) split the whole stream across awkward chunk boundaries
    chunks = []
    sizes = [1, 7, 3, 46, 5, 2, 100, 1, 9]
    pos = 0
    i = 0
    while pos < len(stream):
        take = sizes[i % len(sizes)]
        chunks.append(stream[pos:pos + take])
        pos += take
        i += 1
    out = _drain(serial_parser.FrameExtractor(), chunks)
    assert len(out) == n, "split-stream framing lost frames: %d" % len(out)

    # (b) a truncated tail (partial frame at end of stream) must be dropped
    partial = frames[0][:20]
    out = _drain(serial_parser.FrameExtractor(), [stream + partial])
    assert len(out) == n, "truncated tail produced wrong count: %d" % len(out)

    # (c) a frame cut mid-body then resumed must still parse (buffering)
    cut = len(frames[0]) - 10
    out = _drain(serial_parser.FrameExtractor(),
                 [stream[:cut], stream[cut:]])
    assert len(out) == n, "mid-frame cut/resume lost frames: %d" % len(out)
    return n


def test_noise_resync():
    n = 15
    frames = _frames(n)
    stream = b"".join(frames)

    # deterministic pseudo-random noise (seeded) mixed around the real frames
    rng = random.Random(1234)
    noise = bytes(rng.getrandbits(8) for _ in range(500))

    # prepend noise + interleave noise chunks between every few frames
    parts = [noise]
    for idx, fr in enumerate(frames):
        parts.append(fr)
        if idx % 4 == 3:
            parts.append(bytes(rng.getrandbits(8) for _ in range(64)))
    out = _drain(serial_parser.FrameExtractor(), parts)

    seqs = [p["sequence"] for p in out]
    assert seqs == list(range(n)), (
        "resync after noise lost/reordered frames: %s" % seqs)

    # a false SOF+MAGIC with bad CRC must be rejected, not emitted
    false_sync = bytes([packet.PKT_SOF, packet.PKT_MAGIC]) + b"\x00" * 44
    out = _drain(serial_parser.FrameExtractor(), [false_sync + stream])
    assert len(out) == n, "false sync emitted a bogus frame"
    return n


def main():
    results = []
    for name, fn in [
        ("continuous", test_continuous),
        ("bad_crc", test_bad_crc),
        ("truncated", test_truncated),
        ("noise_resync", test_noise_resync),
    ]:
        try:
            got = fn()
            results.append((name, True, got))
            print("PASS  %-14s (%d packets)" % (name, got))
        except AssertionError as e:
            results.append((name, False, str(e)))
            print("FAIL  %-14s %s" % (name, e))

    ok = all(r[1] for r in results)
    print("offline test:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
