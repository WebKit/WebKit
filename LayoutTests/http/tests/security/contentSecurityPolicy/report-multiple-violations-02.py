#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'unsafe-inline\' \'self\'; report-uri resources/does-not-exist\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html>
<body>
<p>This tests that multiple violations on a page trigger multiple reports
if and only if the violations are distinct. This test passes if only one.
PingLoader callback is visible in the output.</p>
<script>
for (var i = 0; i< 5; i++)
    setTimeout("alert('PASS: setTimeout #" + i + " executed.');", 0);
</script>
</body>
</html>''')
