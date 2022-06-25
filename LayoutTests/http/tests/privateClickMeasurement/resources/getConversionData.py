#!/usr/bin/env python3

import os
import sys
import time
from conversionFilePath import conversion_file_path
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
timeout_ms = query.get('timeout_ms', [None])[0]
end_test = query.get('endTest', [None])[0]

no_timeout = True
timeout_msecs = 0
if timeout_ms is not None:
    no_timeout = False
    timeout_msecs = int(timeout_ms)

conversion_file_found = False
while no_timeout or timeout_msecs > 0:
    if os.path.isfile(conversion_file_path):
        conversion_file_found = True
        break

    sleep_msecs = 10
    time.sleep(sleep_msecs * 0.001)
    if not no_timeout:
        timeout_msecs -= sleep_msecs

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html><body>\n'
)

if conversion_file_found:
    sys.stdout.write('Attribution received.')

    conversion_file = open(conversion_file_path, 'r')
    for line in conversion_file.readlines():
        sys.stdout.write('<br>{}'.format(line.strip()))
    
    sys.stdout.write('<br>')
    
    conversion_file.close()
    if os.path.isfile(conversion_file_path):
        os.remove(conversion_file_path)
else:
    sys.stdout.write('Attribution not received - timed out.<br>')

if end_test is not None:
    sys.stdout.write(
        '<script>'
        'if (window.testRunner) {'
        '    testRunner.notifyDone();'
        '    testRunner.setPrivateClickMeasurementOverrideTimerForTesting(false);'
        '    testRunner.setPrivateClickMeasurementAttributionReportURLsForTesting(\'\', \'\');'
        '}'
        '</script>'
    )

sys.stdout.write('</body></html>')
