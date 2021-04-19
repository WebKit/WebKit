#!/usr/bin/env python3

import os
import re
import sys
import tempfile
from base64 import b64encode
from datetime import datetime, timedelta
from urllib.parse import urlencode, parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies, get_post_data, get_request

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)


def prettify(name):
    return name.strip().lower().replace('http_', '').replace('_', ' ').title().replace(' ', '-')


def decode_multipart(data, boundary):
    data = [line for line in data.split('\r\n')[:-2] if line and boundary not in line]

    key = None
    data_obj = {}
    for line in data:
        if key is None:
            key = re.search(r'\"(.*?)\"', line).group()[1:-1]
        else:
            data_obj[key] = line.strip()
            key = None
    return urlencode(data_obj)


beacon_filename = os.path.join(tempfile.gettempdir(), 'beacon{}.txt'.format(get_request().get('name', '')))
beacon_file = open('{}.tmp'.format(beacon_filename), 'w')

content_type = ''
boundary = None
for name in sorted(os.environ.keys()):
    if name in ['CONTENT_TYPE', 'HTTP_REFERER', 'REQUEST_METHOD', 'HTTP_COOKIE', 'HTTP_ORIGIN']:
        value = os.environ.get(name)
        if name == 'CONTENT_TYPE':
            content_type = value

            boundary = re.search(r'boundary=.*$', value)
            if boundary is not None:
                boundary = boundary.group().split('=')[-1]

            value = re.sub(r'boundary=.*$', r'', value)

        header_name = prettify(name)
        beacon_file.write('{}: {}\n'.format(header_name, value))

post_data = ''.join(sys.stdin.readlines())
if len(post_data) == 0:
    post_data = urlencode(get_post_data())

if boundary is not None:
    post_data = decode_multipart(post_data, boundary)

beacon_file.write('Length: {}\n'.format(len(post_data)))

if 'application/' in content_type:
    post_data = b64encode(post_data.encode()).decode()

beacon_file.write('Body: {}\n'.format(post_data.strip()))
beacon_file.close()
os.rename(beacon_filename + '.tmp', beacon_filename)

if 'dontclearcookies' not in query.keys():
    expires = datetime.utcnow() - timedelta(seconds=60)
    for name in get_cookies().keys():
        sys.stdout.write('Set-Cookie: {}=deleted; expires={} GMT; Max-Age=0; path=/\r\n'.format(name, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))

sys.stdout.write('Content-Type: text/html\r\n\r\n')
