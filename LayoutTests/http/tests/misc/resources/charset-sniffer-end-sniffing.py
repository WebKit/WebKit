#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<html><script>parent.firstScript();</script>\n'
    '<body>\n'
)

for _ in range(0, 10000):
    sys.stdout.write('a')

sys.stdout.flush()
time.sleep(1)

sys.stdout.write(
    '<html><script>parent.secondScript();</script>\n'
    '</body></html>\n'
)