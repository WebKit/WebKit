#!/usr/bin/env python3

import sys
import os

sys.stdout.write(
    'Content-Type: text/vtt\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Access-Control-Allow-Origin: *\r\n\r\n'
)

sys.stdout.write(
     'WEBVTT\r\n'
     '\r\n'
     '1\r\n'
     '00:00:00.000 --> 00:00:01.000\r\n'
     'Sample'
)
