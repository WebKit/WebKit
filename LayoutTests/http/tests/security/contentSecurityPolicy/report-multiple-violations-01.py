#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: img-src \'none\'; report-uri resources/does-not-exist\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html>
<body>
<p>This tests that multiple violations on a page trigger multiple reports.
The test passes if two PingLoader callbacks are visible in the output.</p>
<img src="../resources/abe.png">
<img src="../resources/eba.png">
</body>
</html>''')
