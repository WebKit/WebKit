#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

sys.stdout.write('Access-Control-Allow-Origin: *\r\n')

if os.environ.get('REQUEST_METHOD', '') == 'OPTIONS':
    sys.stdout.write(
        'Access-Control-Allow-Methods: GET\r\n'
        'Access-Control-Allow-Headers: origin, accept-encoding, referer, range\r\n\r\n'
    )
    sys.exit(0)

import media.resources.serve_video
