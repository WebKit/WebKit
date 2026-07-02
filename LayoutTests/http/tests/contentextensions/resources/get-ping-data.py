#!/usr/bin/env python3

import os
import sys
import time
from ping_file_path import ping_filepath
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
timeout_ms = query.get('timeout_ms', [None])[0]
end_test = query.get('end_test', [None])[0]

no_timeout = True
timeout_msecs = 0
if timeout_ms is not None:
    no_timeout = False
    timeout_msecs = int(timeout_ms)

ping_file_found = False
while no_timeout or timeout_msecs > 0:
    if os.path.isfile(ping_filepath):
        ping_file_found = True
        break

    time.sleep(0.01)
    if not no_timeout:
        timeout_msecs -= 10

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html><body>\n'
)

if ping_file_found:
    sys.stdout.write('Ping received.')
    ping_file = open(ping_filepath, 'r')
    for line in ping_file.readlines():
        sys.stdout.write('<br>{}'.format(line.strip()))

    ping_file.close()
    if os.path.isfile(ping_filepath):
        os.remove(ping_filepath)
else:
    sys.stdout.write('Ping not received - timed out.')

if end_test is not None:
    sys.stdout.write(
        '<script>'
        'if (window.testRunner)'
        '    testRunner.notifyDone();'
        '</script>'
    )

sys.stdout.write('</body></html>')