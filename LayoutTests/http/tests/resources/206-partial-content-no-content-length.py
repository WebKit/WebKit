#!/usr/bin/env python3
"""Serves a media file the way the HbbTV HTML52090 test server does.

Every response to /media, even one for a request without a Range header, is
"206 Partial Content" with "Content-Range: bytes first-last/total" and no
Content-Length. The body is sent with chunked transfer encoding in 10 pieces,
1 second apart, so the whole download takes about 10 seconds.

/media?cl=1 adds Content-Length (control case).
/report returns every /media request seen so far, with its Range header.
/result?msg=... prints the page's result on the console (for headless runs).

Usage: range206_server.py MEDIA_FILE [--port 8080]
Then open http://HOST:PORT/ (test.html) in the browser.
"""

import argparse
import json
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

HERE = os.path.dirname(os.path.abspath(__file__))
PIECES = 10
PAUSE = 1.0

lock = threading.Lock()
requests_seen = []
server = None


def log(msg):
    print(time.strftime("%H:%M:%S ") + msg, flush=True)


class WebServerThread(threading.Thread):
    server = None
    started = False
    must_die = False

    def __init__(self, server):
        self.server = server
        threading.Thread.__init__(self)

    def run(self):
        self.server.serve_forever()

    def start_server(self):
        if (self.started):
            return
        self.start()
        self.started = True

    def stop_server(self):
        if (not self.started):
            return
        self.server.shutdown()
        self.started = False


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    media_file = None
    web_server_thread = None

    def log_message(self, fmt, *args):
        pass

    def do_GET(self):
        url = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(url.query, keep_blank_values=True).items()}
        try:
            if url.path == "/media":
                return self.media(q)
            if url.path == "/report":
                with lock:
                    return self.send_body(200, json.dumps(requests_seen), "application/json")
            if url.path == "/result":
                log("RESULT: " + q.get("msg", ""))
                return self.send_body(200, "ok\n", "text/plain")
            if url.path in ("/kill"):
                self.web_server_thread.must_die = True
                return self.send_body(200, "ok\n", "text/plain")
            self.send_body(404, "not found\n", "text/plain")
        except (BrokenPipeError, ConnectionResetError):
            log("  client closed the connection")

    def send_body(self, code, body, ctype):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache, no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def media(self, q):
        total = os.path.getsize(self.media_file)
        range_header = self.headers.get("Range")
        start, end = 0, total - 1
        m = re.match(r"bytes=(\d+)-(\d*)$", range_header or "")
        if m:
            start = int(m.group(1))
            if m.group(2):
                end = min(int(m.group(2)), total - 1)
        length = end - start + 1
        with_cl = q.get("cl") == "1"
        with lock:
            requests_seen.append({"range": range_header, "start": start, "total": total})
        log("GET /media Range=%s -> 206 bytes %d-%d/%d, %s" % (
            range_header, start, end, total, "Content-Length" if with_cl else "chunked, no Content-Length"))

        self.send_response(206)
        self.send_header("Content-Type", "audio/mp4")
        self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, total))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.send_header("Access-Control-Allow-Origin", "*")
        if with_cl:
            self.send_header("Content-Length", str(length))
        else:
            self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()

        piece = length // PIECES
        with open(self.media_file, "rb") as f:
            f.seek(start)
            for i in range(PIECES):
                data = f.read(piece if i < PIECES - 1 else length - piece * (PIECES - 1))
                self.wfile.write((b"%x\r\n" % len(data) + data + b"\r\n") if not with_cl else data)
                self.wfile.flush()
                if i < PIECES - 1:
                    time.sleep(PAUSE)
        if not with_cl:
            self.wfile.write(b"0\r\n\r\n")


def main():
    # This script is designed to run as a CGI to get its arguments, but then it starts its own standalone
    # web server to actually serve the media file having full control.
    sys.stdout.write('Content-Type: text/plain{}\r\n\r\n')

    query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
    media_file = query.get('media_file', [''])[0]
    port = int(query.get('port', ['8206'])[0])
    # How much time the independent server will be active (unless "/kill" is requested before).
    lifetime = int(query.get('lifetime', ['100'])[0])

    Handler.media_file = os.path.abspath(media_file)
    web_server_thread = WebServerThread(ThreadingHTTPServer(("0.0.0.0", port), Handler))
    Handler.web_server_thread = web_server_thread
    log("serving %s on port %d" % (media_file, port))
    web_server_thread.start_server()

    while lifetime > 0 and not web_server_thread.must_die:
        time.sleep(1)
        lifetime = lifetime - 1

    log("Web server stopped. Remaining lifetime: %d" % lifetime)
    web_server_thread.stop_server()
    log("Exiting main script")

    os._exit(os.EX_OK)


if __name__ == "__main__":
    main()
