#!/usr/bin/env python3

import sys
import time

time.sleep(1)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<script>alert(\'PASS - iframe loaded\'); if (window.testRunner) testRunner.notifyDone(); </script>\n'
)