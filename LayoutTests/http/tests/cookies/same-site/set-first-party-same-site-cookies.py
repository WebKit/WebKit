#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.abspath(os.path.dirname(file)))
sys.path.insert(0, http_root)

from resources.cookie_utilities import reset_cookies, should_reset_cookies, wk_set_cookie

sys.stdout.write('Content-Type: text/html\r\n')

if should_reset_cookies():
    reset_cookies()
    sys.stdout.write('\r\n')
    sys.exit(0)

wk_set_cookie('strict', '14', {'SameSite': 'Strict', 'Max-Age': 100, 'path': '/'})
wk_set_cookie('implicit-strict', '14', {'SameSite': None, 'Max-Age': 100, 'path': '/'})
wk_set_cookie('strict-because-invalid-SameSite-value', '14', {'SameSite': 'invalid', 'Max-Age': 100, 'path': '/'})
wk_set_cookie('lax', '14', {'SameSite': 'Lax', 'Max-Age': 100, 'path': '/'})
wk_set_cookie('normal', '14', {'Max-Age': 100, 'path': '/'})
sys.stdout.write('\r\n')

print('''<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}
</script>
<meta http-equiv="refresh" content="0;http://127.0.0.1:8000/cookies/resources/echo-http-and-dom-cookies-and-notify-done.py">
</head>
</html>''')
