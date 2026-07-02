#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
report_only = query.get('report-only', [''])[0]
sandbox = query.get('sandbox', [''])[0]

if report_only:
    sys.stdout.write('Content-Security-Policy-Report-Only: sandbox {}\r\n'.format(sandbox))
else:
    sys.stdout.write('Content-Security-Policy: sandbox {}\r\n'.format(sandbox))

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<p>Ready</p>\n'
    '<script>\n'
    'console.log("Script executed in iframe.");\n'
    'window.secret = "I am a secret";\n'
    '</script>\n'
)