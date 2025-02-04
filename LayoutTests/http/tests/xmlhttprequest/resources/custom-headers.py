#!/usr/bin/env python3
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'X-Custom-Header-Single: single\r\n'
    'X-Custom-Header-Empty:\r\n'
    'X-Custom-Header-List: one\r\n'
    'X-Custom-Header-List: two\r\n'
    '\r\n'
)
