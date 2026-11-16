#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

csp = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('csp', [''])[0]

def determine_content_security_policy_header():
    sys.stdout.write(
        'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
        'Cache-Control: no-cache, must-revalidate\r\n'
        'Pragma: no-cache\r\n'
    )

    if csp:
        sys.stdout.write('Content-Security-Policy: {}\r\n'.format(csp))