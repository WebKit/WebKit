#!/usr/bin/env python3

import cgi
import os
import sys
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

request_method = os.environ.get('REQUEST_METHOD', '')
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

request = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for key in form.keys():
        request.update({ key: form.getvalue(key) })
else:
    query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
    for key in query.keys():
        request.update({ key: query[key][0] })

request.update(get_cookies())

sys.stdout.write('Content-Type: text/html; charset=UTF-8\r\n\r\n')

print('''<html>
<head>
<script>

function runTest()
{
    var r = document.getElementById('result');
    var o = document.getElementById('output').firstChild;
    if (o.nodeValue == \'\u2122\u5341\') 
        r.innerHTML = "SUCCESS: query param is converted to UTF-8";
    else
        r.innerHTML = "FAILURE: query param is not converted to UTF-8. value=" +
        o.nodeValue;
        
    if (window.testRunner)
        testRunner.notifyDone();
}

</script>
</head>
<body onload="runTest()">
<p>
This test is for <a href="http://bugs.webkit.org/show_bug.cgi?id=21635">bug 21635</a>. The query parameter in non-UTF-8 Unicode pages (UTF-7,16,32) should be converted to UTF-8 before a request is made to a server.
</p>''')

sys.stdout.write('<div style=\'display: none;\' id=\'output\'>{}'.format(request.get('q', '')))

print('''</div>
<div id="result"></div>
</body>
</html>''')