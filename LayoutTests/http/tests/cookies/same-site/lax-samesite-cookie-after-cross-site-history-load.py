#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

sys.stdout.write(
    'Cache-Control: no-store\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html>
<body>
<script src="/js-test-resources/js-test.js"></script>
<script>
description("Tests that lax same-site cookies are sent on a cross-site history navigation.");
jsTestIsAsync = true;

onload = function() {''')

if cookies.get('foo', None) is not None:
    sys.stdout.write(
        '    testPassed(\'Was able to read the cookie after the back navigation\');'
        '    finishJSTest();'
        '    return;'
    )
else:
    sys.stdout.write(
        '    document.cookie = \'foo=bar; SameSite=lax\';'
        '    setTimeout(() => { window.location = \'http://localhost:8000/cookies/same-site/resources/navigate-back.html\'; }, 0);'
    )

print('''}
</script>
</body>
</html>''')