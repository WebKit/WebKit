#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies


def hostname_is_equal_to_string(hostname):
    return os.environ.get('HTTP_HOST', '').startswith(hostname)


def reset_cookies_for_current_origin():
    expires = datetime.utcnow() - timedelta(seconds=86400)
    for cookie in get_cookies().keys():
        sys.stdout.write('Set-Cookie: {}=deleted; expires={} GMT; Max-Age=0; path=/\r\n'.format(cookie, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))


def reset_cookies():
    reset_cookies_for_current_origin()
    if hostname_is_equal_to_string('127.0.0.1'):
        sys.stdout.write('Location: http://localhost:8000{}\r\n'.format(os.environ.get('SCRIPT_URL', '')))
    elif hostname_is_equal_to_string('localhost'):
        sys.stdout.write('Location: http://127.0.0.1:8000{}?runTest\r\n'.format(os.environ.get('SCRIPT_URL', '')))


def should_reset_cookies():
    return os.environ.get('QUERY_STRING', None) is None


def wk_set_cookie(name, value, additional_props):
    cookie_value = '{}={}'.format(name, value)
    for prop_name in additional_props:
        cookie_value += '; {}'.format(prop_name)
        if additional_props[prop_name]:
            cookie_value += '={}'.format(additional_props[prop_name])
    sys.stdout.write('Set-Cookie: {}\r\n'.format(cookie_value))
