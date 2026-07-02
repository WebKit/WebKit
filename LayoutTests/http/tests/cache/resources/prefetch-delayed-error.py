#!/usr/bin/env python3

import os
import sys
import time

purpose = os.environ.get('HTTP_SEC_PURPOSE', '')

if purpose == 'prefetch':
    time.sleep(1)
    sys.stdout.write(
        'Status: 503 Service Unavailable\r\n'
        'Cache-Control: no-store\r\n'
        'Content-Type: text/html\r\n\r\n'
        '<!DOCTYPE html><html><body>503 Error</body></html>\n'
    )
else:
    sys.stdout.write(
        'Cache-Control: no-store\r\n'
        'Content-Type: text/html\r\n\r\n'
        '<!DOCTYPE html><html><body>'
        '<script>if (window.testRunner) testRunner.notifyDone();</script>'
        'PASS</body></html>\n'
    )
