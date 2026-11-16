#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

boundary = 'cutHere'
def send_part(data):
    sys.stdout.buffer.write(b'Content-Type: image/png\r\n\r\n')
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.write('\r\n--{}\r\n'.format(boundary).encode())
    sys.stdout.flush()

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
interval = query.get('interval', [None])[0]
loop = query.get('loop', [None])[0]
wait = query.get('wait', [None])[0]

i = 1
images = []
while True:
    img = query.get('img{}'.format(i), [None])[0]
    if img is None:
        break

    with open(os.path.join(os.path.dirname(__file__), img), 'rb') as file:
        images.append(file.read())
    i += 1

if interval is not None:
    interval = float(interval)
else:
    interval = 1

sys.stdout.buffer.write(
    'Content-Type: multipart/x-mixed-replace; boundary={boundary}\r\n\r\n'
    '--{boundary}\r\n'.format(boundary=boundary).encode()
)
sys.stdout.flush()

while True:
    for k in range(1, i):
        send_part(images[k - 1])
        time.sleep(interval)

    if loop is None:
        break

if wait is not None:
    time.sleep(float(wait))
