#!/usr/bin/env python3

import os
import sys

sys.stdout.write('Access-Control-Allow-Origin: *\r\n')

if os.environ.get('REQUEST_METHOD', '') == 'OPTIONS':
    sys.stdout.write(
        'Access-Control-Allow-Methods: GET\r\n'
        'Access-Control-Allow-Headers: origin, accept-encoding, referer, range\r\n\r\n'
    )
    sys.exit(0)

import serve_video  # noqa: E402 — must run after CORS headers are emitted
