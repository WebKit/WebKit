#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<html>
<head>
<script>
function runTest() {

    if (window.testRunner)
        testRunner.dumpAsText();''')

if cookies.get('one_cookie', None) is not None:
    sys.stdout.write('    document.write("FAIL: Cookies with a wrong domain should be rejected in user agent.");')
else:
    sys.stdout.write('    document.write("PASS: User agent rejected the cookie with a wrong domain.")')

print('''}
</script>
</head>
<body onload="runTest()">
</body>
</html>''')
