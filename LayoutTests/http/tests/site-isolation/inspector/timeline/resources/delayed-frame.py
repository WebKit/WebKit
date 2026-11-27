#!/usr/bin/env python3

# Serves a frame document after a delay, so that the frame stays on its initial about:blank document for a while.

import sys
import time

time.sleep(0.5)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    '\r\n'
    '<!DOCTYPE html>\n'
    '<meta charset="utf-8">\n'
    '<p>Delayed frame for Timeline tests.</p>\n'
)
