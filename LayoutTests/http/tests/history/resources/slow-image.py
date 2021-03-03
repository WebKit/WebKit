#!/usr/bin/env python3

import sys
import time

time.sleep(30)

sys.stdout.write(
    'Location: data:image/gif;base64,R0lGODlhAQABAJAAAMjIyAAAACwAAAAAAQABAAACAgQBADs%3D\r\n'
    'status: 200\r\n'
    'Content-Type: text/html\r\n\r\n'
)