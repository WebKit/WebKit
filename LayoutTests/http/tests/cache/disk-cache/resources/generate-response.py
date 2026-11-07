#!/usr/bin/env python3

import os
import re
import sys
import time
from email.utils import formatdate
from random import randint
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)


def parameter(name):
    return query.get(name, [''])[0]


body = parameter('body')
if body == 'unique':
    body = f'{randint(0, 0xffffffff):08X}\n'

has_status_code = False
has_location = False
status = parameter('Status')
if status in ('301', '302', '303', '307') and parameter('Location') == 'unique-cacheable':
    sys.stdout.write(
        f'Status: {status}\r\n'
        f'Location: generate-response.py?body={randint(0, 0xffffffff):08X}&Cache-control=max-age%3D1000&uniqueId={parameter("uniqueId")}\r\n'
    )
    has_location = True
    has_status_code = True

has_expires_header = False
if os.environ.get('HTTP_IF_NONE_MATCH') == 'match':
    sys.stdout.write('Status: 304\r\n')
    has_status_code = True
    if parameter('expires-in-future-in-304'):
        sys.stdout.write(f'Expires: {formatdate(time.time() + 100, usegmt=True)}\r\n')
        has_expires_header = True

byte_range = re.search(r'bytes=(\d+)-(\d+)', parameter('Range'))
if byte_range:
    if int(byte_range.group(1)) < 6 and int(byte_range.group(2)) < 6:
        sys.stdout.write('Status: 206\r\n')
    else:
        sys.stdout.write('Status: 416\r\n')
    has_status_code = True

for name, values in query.items():
    if name in ('uniqueId', 'delay', 'body'):
        continue
    if (name == 'Location' and has_location) or (name == 'Status' and has_status_code) or (name == 'Expires' and has_expires_header):
        continue
    sys.stdout.write(f'{name}: {values[0]}\r\n')
sys.stdout.write('\r\n')

delay = parameter('delay')
if delay:
    # Padding makes the headers and the body arrive separately.
    sys.stdout.write(' ' * 80 * 1024)
    sys.stdout.flush()
    time.sleep(float(delay))

sys.stdout.write(body)
