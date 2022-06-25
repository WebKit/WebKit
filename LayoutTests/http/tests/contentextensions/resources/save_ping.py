#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from ping_file_path import ping_filepath

def not_being_called():
    cookies = {}
    expires = datetime.utcnow() - timedelta(seconds=60)
    if 'HTTP_COOKIE' in os.environ:
        header_cookies = os.environ['HTTP_COOKIE']
        header_cookies = header_cookies.split('; ')

        for cookie in header_cookies:
            cookie = cookie.split('=')
            sys.stdout.write('Set-Cookie: {}=deleted; expires={} GMT; Max-Age=0; path=/\r\n'.format(cookie[0], expires.strftime('%a, %d-%b-%Y %H:%M:%S')))

    sys.stdout.write('Content-Type: text/html\r\n\r\n')

def save_ping(is_being_called):
    ping_file = open('{}.tmp'.format(ping_filepath), 'w')
    cookies_found = False

    for name in os.environ.keys():
        if name == 'HTTP_HOST':
            ping_file.write('{}: {}\n'.format(name, os.environ.get(name)))
        elif name == 'REQUEST_URI':
            uri = os.environ.get(name)
            if is_being_called:
                uri = '/contentextensions/resources/save-ping.py?{}'.format(uri.split('?')[-1])
                ping_file.write('{}: {}\n'.format(name, uri))
            else:
                ping_file.write('{}: {}\n'.format(name, uri))
        elif name == 'HTTP_COOKIE':
            ping_file.write('Cookies in ping: {}\n'.format(os.environ.get(name)))
            cookies_found = True

    if not cookies_found:
        ping_file.write('No cookies in ping.\n')

    ping_file.close()
    if os.path.isfile('{}.tmp'.format(ping_filepath)):
        os.rename('{}.tmp'.format(ping_filepath), ping_filepath)

    if not is_being_called:
        not_being_called()