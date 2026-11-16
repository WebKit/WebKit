#!/usr/bin/env python3

import os
import sys
from urllib.parse import unquote


def add_cache_control(allow_cache):
    if allow_cache is not None:
        sys.stdout.write('Cache-Control: public, max-age=86400\r\n\r\n')
    else:
        # Workaround for https://bugs.webkit.org/show_bug.cgi?id=77538
        # Caching redirects results in flakiness in tests that dump loader delegates.
        sys.stdout.write('Cache-Control: no-store\r\n\r\n')


def set_cookie(cookies):
    for cookie in cookies:
        sys.stdout.write('Set-Cookie: {}\r\n'.format(cookie.replace(':', '=')))


def add_cors(allowHeaders):
    sys.stdout.write(
        'Access-Control-Allow-Credentials: true\r\n'
        'Access-Control-Allow-External: true\r\n'
        'Access-Control-Allow-Headers: {}\r\n'
        'Access-Control-Allow-Origin: {}\r\n'.format(allowHeaders, os.environ.get('HTTP_ORIGIN', ''))
    )


query = {}
for key_value in os.environ.get('QUERY_STRING', '').split('&'):
    arr = key_value.split('=')
    if len(arr) > 1:
        if query.get(arr[0], None) is not None and arr[1] not in query[arr[0]]:
            query[arr[0]].append(unquote('='.join(arr[1:])))
        else:
            query[arr[0]] = [unquote('='.join(arr[1:]))]
    else:
        query[arr[0]] = ['']


allow_cache = query.get('allowCache', [None])[0]
code = int(query.get('code', [302])[0])
refresh = query.get('refresh', [None])[0]
url = query.get('url', [''])[0]
cookies = query.get('cookie', [''])
allow_cross_origin = query.get('allowCrossOrigin', [None])[0]
allow_headers = query.get('allowHeaders', [None])[0]

sys.stdout.write('Content-Type: text/html\r\n')

if allow_cross_origin:
    add_cors(allow_headers)

if os.environ.get('REQUEST_METHOD', 'GET') == 'OPTIONS':
    sys.stdout.write('status: 200\r\n\r\n')
    sys.exit(0)

if refresh is not None:
    sys.stdout.write(
        'status: 200\r\n'
        'Refresh: {}; url={}\r\n'.format(refresh, url)
    )

    set_cookie(cookies)
    add_cache_control(allow_cache)
    sys.exit(0)

sys.stdout.write(
    'status: {}\r\n'
    'Location: {}\r\n'.format(code, url)
)

set_cookie(cookies)
add_cache_control(allow_cache)
