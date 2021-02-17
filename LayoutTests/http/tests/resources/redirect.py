#!/usr/bin/env python3
import os
import sys

from urllib.parse import unquote


def addCacheControl(allowCache=True):
    if allowCache:
        sys.stdout.write('Cache-Control: public, max-age=86400\r\n')
    else:
        sys.stdout.write('Cache-Control: no-store\r\n')

sys.stdout.write('Content-Type: text/html\r\n')

query = {}
for part in os.environ.get('QUERY_STRING', '').split('&'):
    if not part:
        continue
    query[part.split('=')[0]] = '='.join(part.split('=')[1:])

url = unquote(query.get('url', None))
allowCache = True if query.get('allowCache', None) is not None else False
refresh = query.get('refresh', None)
code = query.get('code', '302')

if refresh:
    sys.stdout.write(
        'status: 200\r\n'
        'Refresh: {}; url={}\r\n'.format(refresh, url)
    )
    addCacheControl(allowCache)
    sys.stdout.write('\r\n')
    sys.exit(0)

sys.stdout.write('status: {}\r\n'.format(code))
sys.stdout.write('Location: {}\r\n'.format(url))
addCacheControl(allowCache)
sys.stdout.write('\r\n')

