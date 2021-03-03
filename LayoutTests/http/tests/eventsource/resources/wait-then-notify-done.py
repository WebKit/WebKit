#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-store, no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n\r\n'
)

time.sleep(1)

print('<script> if (window.testRunner) testRunner.notifyDone()</script>')