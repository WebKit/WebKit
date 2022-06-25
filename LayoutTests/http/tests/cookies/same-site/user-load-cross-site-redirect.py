#!/usr/bin/env python3

import json
import os
import sys
from urllib.parse import quote

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

http_root = os.path.dirname(os.path.abspath(os.path.dirname(file)))
sys.path.insert(0, http_root)

from resources.cookie_utilities import hostname_is_equal_to_string, wk_set_cookie

cookies = get_cookies()

sys.stdout.write('Content-Type: text/html\r\n')

if hostname_is_equal_to_string('127.0.0.1') and os.environ.get('QUERY_STRING', '') == '':
    wk_set_cookie('strict', '27', {'SameSite': 'Strict', 'Max-Age': 100, 'path': '/'})
    wk_set_cookie('lax', '27', {'SameSite': 'Lax', 'Max-Age': 100, 'path': '/'})
    wk_set_cookie('normal', '27', {'Max-Age': 100, 'path': '/'})

    path = quote('http://127.0.0.1:8000{}?check-cookies'.format(os.environ.get('REQUEST_URI')))
    sys.stdout.write('Location: http://localhost:8000/resources/redirect.py?url={}\r\n\r\n'.format(path))
    sys.exit(0)

sys.stdout.write('\r\n')
print('''<!DOCTYPE html>
<html>
<head>
<script src="/js-test-resources/js-test.js"></script>
<script src="../resources/cookie-utilities.js"></script>
<script>_setCachedCookiesJSON('{}')</script>
</head>
<body>
<script>
window.jsTestIsAsync = true;

description("This test is representative of a user that loads a site, via the address bar or Command + clicking a hyperlink, that redirects to a cross-site page that expects its SameSite Lax cookies.");

async function checkResult()
{{
    debug("Cookies sent with HTTP request:");
    // FIXME: Ensure that strict cookies are not sent. See <https://bugs.webkit.org/show_bug.cgi?id=194933>.
    await shouldHaveCookieWithValue("lax", "27");
    await shouldHaveCookieWithValue("normal", "27");

    debug("<br>Cookies visible in DOM:");
    shouldHaveDOMCookieWithValue("strict", "27");
    shouldHaveDOMCookieWithValue("lax", "27");
    shouldHaveDOMCookieWithValue("normal", "27");

    await resetCookies();
    finishJSTest();
}}

checkResult();
</script>
</body>
</html>'''.format(json.dumps(cookies)))
