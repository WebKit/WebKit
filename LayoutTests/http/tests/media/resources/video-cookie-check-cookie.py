#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()
test = cookies.get('TEST', None)

sys.stdout.write(
    'Access-Control-Allow-Origin: *\r\n'
)


filename = 'test.'
if test is not None:
    extension = test.split('.')[-1]

    if extension == 'mp4':
        sys.stdout.write('Content-Type: video/mp4\r\n')
        filename += extension
    elif extension == 'ogv':
        sys.stdout.write('Content-Type: video/ogg\r\n')
        filename += extension
    elif extension == 'ts':
        sys.stdout.write('Content-Type: video/mpegts\r\n')
        filename = 'hls/test.ts'
    else:
        sys.stdout.write('Content-Type: text/html\r\n\r\n')
        sys.exit(0)

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Connection: close\r\n'
)

if os.path.isfile(filename):
    sys.stdout.write('Content-Length: {}\r\n\r\n'.format(os.path.getsize(filename)))

    sys.stdout.flush()
    with open(os.path.join(os.path.dirname(__file__), filename), 'rb') as file:
        sys.stdout.buffer.write(file.read())

else:
    sys.stdout.write('\r\n')
    sys.stderr.write('Could not find file {}\n'.format(filename))
