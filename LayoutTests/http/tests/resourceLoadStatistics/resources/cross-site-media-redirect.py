#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

cmd = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('cmd', [''])[0]
log_path = os.path.join(tempfile.gettempdir(), 'rdar181913390-media-cookie.txt')

if cmd == 'clear':
    open(log_path, 'w').close()
    sys.stdout.write('Content-Type: text/plain\r\nCache-Control: no-store\r\n\r\n')
    sys.exit(0)

if cmd == 'get':
    had_cookie = False
    try:
        with open(log_path) as log:
            had_cookie = 'SESSIONID' in log.read()
    except FileNotFoundError:
        pass
    sys.stdout.write('Content-Type: text/html\r\nCache-Control: no-store\r\n\r\n')
    sys.stdout.write('Redirected cross-site media request: {}.<br>'.format('received third-party cookie' if had_cookie else 'no cookie'))
    sys.exit(0)

if cmd == 'serve':
    with open(log_path, 'a') as log:
        log.write(os.environ.get('HTTP_COOKIE', '') + '\n')

    media_file = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'media', 'resources', 'test.mp4')
    file_size = os.path.getsize(media_file)
    start, end, status = 0, file_size - 1, '200'
    http_range = os.environ.get('HTTP_RANGE')
    if http_range and http_range.startswith('bytes='):
        bounds = http_range[len('bytes='):].split('-')
        if bounds[0]:
            start = int(bounds[0])
        if len(bounds) > 1 and bounds[1]:
            end = int(bounds[1])
        status = '206'

    sys.stdout.write('status: {}\r\n'.format(status))
    sys.stdout.write('Content-Type: video/mp4\r\n')
    sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
    sys.stdout.write('Cache-Control: no-store\r\n')
    sys.stdout.write('Accept-Ranges: bytes\r\n')
    sys.stdout.write('Content-Length: {}\r\n'.format(end - start + 1))
    if status == '206':
        sys.stdout.write('Content-Range: bytes {}-{}/{}\r\n'.format(start, end, file_size))
    sys.stdout.write('\r\n')
    sys.stdout.flush()
    with open(media_file, 'rb') as media:
        media.seek(start)
        sys.stdout.buffer.write(media.read(end - start + 1))
    sys.exit(0)

scheme = 'https' if os.environ.get('HTTPS') else 'http'
location = '{}://{}{}?cmd=serve'.format(scheme, os.environ.get('HTTP_HOST', ''), os.environ.get('SCRIPT_NAME', ''))
sys.stdout.write('status: 302\r\nLocation: {}\r\nCache-Control: no-store\r\nContent-Type: text/plain\r\n\r\n'.format(location))
