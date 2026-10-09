#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: upgrade-insecure-requests\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<script>\n'
    'parent.postMessage("loaded", "*");\n'
    '</script>\n'
)
