#!/usr/bin/env python3

import os
import sys
import time
from tokenSigningFilePath import token_signing_filepath
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

token_signing_file_found = False
while no_timeout or timeout_msecs > 0:
    if os.path.isfile(token_signing_filepath):
        token_signing_file_found = True
        break

    time.sleep(0.01)
    if not no_timeout:
        timeout_msecs -= 10

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html><body>\n'
)

if token_signing_file_found:
    sys.stdout.write('Token signing request received.')

    token_signing_file = open(token_signing_filepath, 'r')
    for line in token_signing_file.readlines():
        sys.stdout.write('<br>{}'.format(line.strip()))
    sys.stdout.write('<br>')

    token_signing_file.close()
    if os.path.isfile(token_signing_filepath):
        os.remove(token_signing_filepath)
else:
    sys.stdout.write('Token signing request not received - timed out.<br>')

if end_test is not None:
    sys.stdout.write(
        '<script>'
        'if (window.testRunner) {'
        '    testRunner.setPrivateClickMeasurementOverrideTimerForTesting(false);'
        '    testRunner.setPrivateClickMeasurementTokenPublicKeyURLForTesting(\'\');'
        '    testRunner.setPrivateClickMeasurementTokenSignatureURLForTesting(\'\');'
        '    testRunner.notifyDone();'
        '}'
        '</script>'
    )

sys.stdout.write('</body></html>')