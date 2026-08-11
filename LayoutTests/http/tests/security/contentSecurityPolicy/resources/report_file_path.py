#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs, urlparse

referer = os.environ.get('HTTP_REFERER', None)
test = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('test', [None])[0]

if test is not None:
    report_filepath = os.path.join(tempfile.gettempdir(), '{}.csp-report.txt'.format(test.replace('/', '-')))
elif referer is not None and '/resources/' not in referer:
    report_filepath = os.path.join(tempfile.gettempdir(), '{}.csp-report.txt'.format(urlparse(referer).path.replace('/', '-')))
else:
    sys.stdout.write(
        'status: 500\r\n'
        'Content-Type: text/html\r\n\r\n'
        'This script needs to know the name of the test to form a unique temporary file path. It can get one either from HTTP referrer, or from a \'test\' parameter.\n'
    )
    sys.exit(0)
