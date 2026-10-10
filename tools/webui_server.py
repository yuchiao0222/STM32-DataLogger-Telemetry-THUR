#!/usr/bin/env python3
"""AEBS telemetry WebUI server.

Reads the telemetry stream from a serial port (or file) and serves a live
dashboard over HTTP + WebSocket.

Usage:
    python webui_server.py --port COM3 --baud 9600 --web 8080
    Then open http://localhost:8080 in a browser.

Dependencies:
    pip install pyserial aiohttp
"""

import argparse
import asyncio
import json
import os
import sys
import threading
import time

import packet
import serial_parser

try:
    from aiohttp import web
except ImportError:
    sys.exit("缺少 aiohttp，請先執行：pip install aiohttp")

INDEX_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "webui", "index.html")

SNAPSHOT = {
    "sequence": None,
    "timestamp_ms": None,
    "fields": {},
    "packets": 0,
    "last_update": 0.0,
    "connected": False,
}
_lock = threading.Lock()
_clients = set()


def reader_loop(port, baud):
    """Background thread: read serial, parse frames, update SNAPSHOT."""
    import serial

    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as exc:
        print("[webui] 串口開啟失敗: %s" % exc)
        return

    print("[webui] 已開啟 %s @ %d baud" % (port, baud))
    extractor = serial_parser.FrameExtractor()

    while True:
        try:
            data = ser.read(1024)
        except Exception:
            break
        if not data:
            continue

        for p in extractor.feed(data):
            dec = packet.decode_fields(p["fields"])
            with _lock:
                SNAPSHOT["sequence"] = p["sequence"]
                SNAPSHOT["timestamp_ms"] = p["timestamp_ms"]
                SNAPSHOT["fields"] = dec
                SNAPSHOT["packets"] += 1
                SNAPSHOT["last_update"] = time.time()
                SNAPSHOT["connected"] = True


async def index(request):
    return web.FileResponse(INDEX_PATH)


async def ws_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    _clients.add(ws)
    try:
        # send current snapshot immediately on connect
        with _lock:
            payload = json.dumps(SNAPSHOT, ensure_ascii=False)
        await ws.send_str(payload)
        async for _ in ws:
            pass
    finally:
        _clients.discard(ws)
    return ws


async def broadcaster():
    while True:
        await asyncio.sleep(0.5)
        with _lock:
            payload = json.dumps(SNAPSHOT, ensure_ascii=False)
        for ws in list(_clients):
            try:
                await ws.send_str(payload)
            except Exception:
                _clients.discard(ws)


async def app_ctx(app):
    task = asyncio.create_task(broadcaster())
    yield
    task.cancel()


def main(argv):
    ap = argparse.ArgumentParser(description="AEBS telemetry WebUI")
    ap.add_argument("--port", help="serial port, e.g. COM3")
    ap.add_argument("--file", help="raw binary log file instead of serial")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--web", type=int, default=8080, help="HTTP port (default 8080)")
    args = ap.parse_args(argv)

    if not args.port and not args.file:
        ap.error("需要 --port COMx 或 --file raw.bin")

    app = web.Application()
    app.cleanup_ctx.append(app_ctx)
    app.router.add_get("/", index)
    app.router.add_get("/ws", ws_handler)

    if args.file:
        # 離線重播模式（方便測試前端，不需硬體）
        print("[webui] 離線重播模式: %s" % args.file)
        threading.Thread(target=replay_loop, args=(args.file,), daemon=True).start()
    else:
        threading.Thread(target=reader_loop, args=(args.port, args.baud), daemon=True).start()

    print("[webui] 瀏覽器開啟: http://localhost:%d" % args.web)
    web.run_app(app, host="0.0.0.0", port=args.web)


def replay_loop(path):
    """Replay a raw log file at ~2 packets/s for UI testing."""
    import packet as _p
    f = open(path, "rb")
    extractor = serial_parser.FrameExtractor()
    last = 0.0
    while True:
        data = f.read(1024)
        if not data:
            break
        for p in extractor.feed(data):
            now = time.time()
            if now - last < 0.5:
                time.sleep(0.5 - (now - last))
            last = time.time()
            dec = _p.decode_fields(p["fields"])
            with _lock:
                SNAPSHOT["sequence"] = p["sequence"]
                SNAPSHOT["timestamp_ms"] = p["timestamp_ms"]
                SNAPSHOT["fields"] = dec
                SNAPSHOT["packets"] += 1
                SNAPSHOT["last_update"] = time.time()
                SNAPSHOT["connected"] = True


if __name__ == "__main__":
    main(sys.argv[1:])
