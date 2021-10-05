#!/usr/bin/env python3
import base64
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n\r\n')
if os.environ.get('HTTP_AUTHORIZATION'):
    sys.stdout.write(os.environ['HTTP_AUTHORIZATION'])
else:
    sys.stdout.write('Missing Authorization header')
sys.stdout.flush()
