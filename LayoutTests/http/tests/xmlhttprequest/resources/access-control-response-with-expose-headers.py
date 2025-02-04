#!/usr/bin/env python3
import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Access-control-max-age: 0\r\n'
    'Access-control-allow-origin: *\r\n'
    'X-foo: BAR\r\n'
    'X-TEST: TEST\r\n'
    'Access-Control-Expose-Headers: x-Foo\r\n'
    'Content-Type: text/html\r\n'
    '\r\n'
    'echo\n'
)
