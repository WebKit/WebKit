#!/usr/bin/env python3

import sys
from uuid import uuid1

cookie = f'speculativeRequestValidation={uuid1()}'

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: max-age=0\r\n'
    'Etag: 123456789\r\n'
    f'Set-Cookie: {cookie}\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<body>\n'
    '<script src="request-headers-script.py"></script>\n'
    '<script>\n'
    f'sentSetCookieHeader = \'{cookie}\';\n'
    '</script>\n'
    '</body>\n'
)