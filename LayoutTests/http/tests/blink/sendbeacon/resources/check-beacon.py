#!/usr/bin/env python3

import os
import sys
import tempfile
import time

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_request


def extensive_strip(text):
    # Sometimes the string ends with \x00
    # and .strip() does not remove it
    text = text.strip()
    while text[-1].encode() == b'\x00':
        text = text[:-1]

    return text


request = get_request()
beacon_filename = os.path.join(tempfile.gettempdir(), 'beacon{}.txt'.format(request.get('name', '')))

max_attempts = 700
retries = int(request.get('retries', max_attempts))
while not os.path.isfile(beacon_filename) and retries != 0:
    time.sleep(0.01)
    retries -= 1

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'Access-Control-Allow-Origin: *\r\n\r\n'
)

if os.path.isfile(beacon_filename):
    with open(beacon_filename, 'r') as beacon_file:
        sys.stdout.write('Beacon sent successfully\n')
        for line in beacon_file.readlines():
            trimmed = extensive_strip(line)
            if trimmed != '':
                sys.stdout.write('{}\n'.format(trimmed))

    os.remove(beacon_filename)
else:
    sys.stdout.write('Beacon not sent\n')
