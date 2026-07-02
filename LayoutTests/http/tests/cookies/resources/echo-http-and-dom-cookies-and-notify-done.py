#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

skipNotifyDone = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('skipNotifyDone', [None])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<!DOCTYPE html>
<html>
<head>
<script src="cookie-utilities.js"></script>
<script>
function logDOMCookie(name, value)
{
    document.getElementById("dom-cookies-output").appendChild(document.createTextNode(`${name} = ${value}\n`));
}

window.onload = () => {
    let domCookies = getDOMCookies();
    for (let name of Object.keys(domCookies).sort())
        logDOMCookie(name, domCookies[name]);
''')

if not skipNotifyDone:
    print('''if (window.testRunner)
        testRunner.notifyDone();
    ''')

print('''
};
</script>
</head>
<body>
<p>HTTP sent cookies:</p>
<pre>''')

sorted_cookie_names = sorted(cookies.keys())
for name in sorted_cookie_names:
    sys.stdout.write('{} = {}\n'.format(name, cookies.get(name)))

print('''</pre>
<p>DOM cookies:</p>
<pre id="dom-cookies-output"></pre>
</body>
</html>''')