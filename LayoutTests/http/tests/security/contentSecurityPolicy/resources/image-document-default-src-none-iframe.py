#!/usr/bin/env python3

import os
import sys

filename = '../../../resources/square.png'
filesize = os.path.getsize(filename)
handle = open(filename, 'rb')
contents = handle.read()
handle.close()

sys.stdout.write(
    'Content-Security-Policy: default-src \'none\'\r\n'
    'Content-Length: {}\r\n'
    'Content-Type: image/png\r\n\r\n{}'.format(filesize, contents)
)