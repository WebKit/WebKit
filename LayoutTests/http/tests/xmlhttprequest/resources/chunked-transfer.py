#!/usr/bin/env python3
import os
import sys

sys.stdout.buffer.write(
    b'Content-Type: text/html\r\n'
    b'Transfer-encoding: chunked\r\n'
    b'Cache-Control: no-cache, no-store\r\n'
    b'\r\n'
)

sys.stdout.flush()
sys.stdout.buffer.write(b"4\r\n<a/>\r\n")
sys.stdout.flush()
sys.stdout.buffer.write(b"0\r\n\r\n")
