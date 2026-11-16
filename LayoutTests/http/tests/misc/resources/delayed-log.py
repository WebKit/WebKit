#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
delay = int(query.get('delay', [0])[0])
msg = query.get('msg', [''])[0]
done = query.get('done', [''])[0]

sys.stdout.write('Content-Type: text/javascript\r\n\r\n')
time.sleep(delay)
sys.stdout.write('log(\'{}\');\n'.format(msg))
if done == '1':
    sys.stdout.write(
        'if (window.testRunner)\n'
        '    testRunner.notifyDone();\n'
    )