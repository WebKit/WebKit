#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

test = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('test', [''])[0]

sys.stdout.write(
    'Content-Security-Policy: script-src \'self\'; report-uri save-report.py?test={}\r\n'
    'Content-Type: text/html\r\n\r\n'.format(test)
)

print('''<script>
// This script block will trigger a violation report.
alert('FAIL');
</script>
<script src="go-to-echo-report.py?test={}"></script>'''.format(test))
