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

description("Tests that a SameSite Lax cookie for 127.0.0.1 is sent with a top-level navigation initiated from a page with a different origin.");

async function checkResult()
{{
    debug("Cookies sent with HTTP request:");
    await shouldNotHaveCookie("strict");
    await shouldHaveCookieWithValue("implicit-strict", "5");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "5");
    await shouldHaveCookieWithValue("lax", "5");

    debug("<br>Cookies visible in DOM:");
    shouldHaveDOMCookieWithValue("strict", "5");
    shouldHaveDOMCookieWithValue("implicit-strict", "5");
    shouldHaveDOMCookieWithValue("strict-because-invalid-SameSite-value", "5");
    shouldHaveDOMCookieWithValue("lax", "5");

    await resetCookies();
    finishJSTest();
}}

checkResult();
</script>
</body>
</html>'''.format(json.dumps(cookies)))
