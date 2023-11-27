#!/usr/bin/env python3

from http.server import SimpleHTTPRequestHandler, HTTPServer
import argparse
import logging
import sys
import threading


_log = logging.getLogger(__name__)


class Handler(SimpleHTTPRequestHandler):
    web_root = None

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs, directory=self.web_root)

    def server_shutdown(self):
        try:
            shutdown_thread = threading.Thread(target=self.server.shutdown)
            shutdown_thread.start()
        except Exception as e:
            _log.error(f'Error shutting down HTTP server: {e}')
            sys.exit(1)

    def log_message(self, format, *args):
        _log.info(format % args)

    def do_GET(self):
        shutdown_path = self.web_root + '/shutdown'
        if not self.translate_path(self.path).startswith(shutdown_path):
            super().do_GET()
            return
        self.send_response(200)
        self.end_headers()
        self.server_shutdown()

    def do_POST(self):
        report_path = self.web_root + '/report'
        if not self.translate_path(self.path).startswith(report_path):
            super().do_POST()
            return
        length = int(self.headers['content-length'])
        try:
            content = self.rfile.read(length)
            sys.stdout.write(content.decode('utf-8'))
            sys.stdout.flush()
            self.send_response(200)
        except Exception as e:
            _log.error(f'Error reading and writing report: {e}')
            self.send_response(500)
        self.end_headers()
        self.server_shutdown()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='python py3_builtin_http_server.py web_root')
    parser.add_argument('web_root')
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--interface', default='')
    parser.add_argument('--log-path', default='/tmp/run-benchmark-http.log')
    args = parser.parse_args()

    logging.basicConfig(datefmt='%Y-%m-%d %H:%M:%S',
                        format='%(asctime)s [%(levelname)s]: %(message)s', filename=args.log_path, level=logging.INFO)
    port = args.port
    Handler.web_root = args.web_root
    httpd = HTTPServer((args.interface, port), Handler)
    httpd.serve_forever()
