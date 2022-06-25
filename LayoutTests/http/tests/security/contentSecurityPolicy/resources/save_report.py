#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from report_file_path import report_filepath

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies


def not_being_called():
    cookies = get_cookies()
    expires = datetime.utcnow() - timedelta(seconds=60)
    for cookie in cookies.keys():
        sys.stdout.write('Set-Cookie: {}=deleted; expires={} GMT; Max-Age=0; path=/\r\n'.format(cookie, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))


def save_report(is_being_called):
    data = ''.join(sys.stdin.readlines())

    report_file = open('{}.tmp'.format(report_filepath), 'w')

    for name in sorted(os.environ.keys()):
        if name in ['CONTENT_TYPE', 'HTTP_REFERER', 'REQUEST_METHOD', 'HTTP_COOKIE', 'HTTP_HOST', 'REQUEST_URI']:
            report_file.write('{}: {}\n'.format(name, os.environ.get(name)))

    report_file.write('=== POST DATA ===\n{}'.format(data))
    report_file.close()

    os.rename('{}.tmp'.format(report_filepath), report_filepath)

    if not is_being_called:
        not_being_called()

    sys.stdout.write('Content-Type: text/html\r\n\r\n')
