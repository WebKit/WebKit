#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Cache-Control: max-age=0\r\n'
    'Etag: 123456789\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<body>\n'
    '<script src="resource-with-auth.py"></script>\n'
    '</body>\n'
)