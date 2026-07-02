#!/usr/bin/env python3

import os
import sys

filename = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))), 'resources', 'square.png')

if not os.path.isfile(filename):
    sys.stderr.write('File {} does not exist\n'.format(filename))
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
    sys.exit(0)

filesize = os.path.getsize(filename)
handle = open(filename, 'rb')
contents = handle.read()
handle.close()

sys.stdout.write(
    'Content-Security-Policy: default-src \'none\'\r\n'
    'Content-Length: {}\r\n'
    'Content-Type: image/png\r\n\r\n{}'.format(filesize, contents)
)
