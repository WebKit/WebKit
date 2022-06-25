#!/usr/bin/env python3
import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Transfer-encoding: chunked\r\n'
    'Cache-Control: no-cache, no-store\r\n'
    '\r\n'
)

sys.stdout.flush()
sys.stdout.write("4\r\n<a/>\r\n")
sys.stdout.flush()
sys.stdout.write("0\r\n\r\n")
