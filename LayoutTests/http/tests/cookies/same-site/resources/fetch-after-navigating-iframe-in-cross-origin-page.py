#!/usr/bin/env python3

import json
import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<!DOCTYPE html>
<html>
<head>
<script src="/js-test-resources/js-test.js"></script>
<script src="../../resources/cookie-utilities.js"></script>
<script>_setCachedCookiesJSON('{}')</script>
</head>
<body>
<script>
window.jsTestIsAsync = true;

description("Tests that Same-Site cookies for 127.0.0.1 are not sent with a frame navigation for a frame embedded in a page with a different origin.");

async function checkResult()
{{
    debug("Cookies sent with HTTP request:");
    await shouldNotHaveCookie("strict");
    await shouldNotHaveCookie("implicit-lax", "6");
    await shouldNotHaveCookie("lax-because-invalid-SameSite-value", "6");
    await shouldNotHaveCookie("lax");

    debug("<br>Cookies visible in DOM:");
    shouldNotHaveDOMCookie("strict");
    shouldNotHaveDOMCookie("implicit-lax", "6");
    shouldNotHaveDOMCookie("lax-because-invalid-SameSite-value", "6");
    shouldNotHaveDOMCookie("lax");

    await resetCookies();
    finishJSTest();
}}

checkResult();
</script>
</body>
</html>'''.format(json.dumps(cookies)))
