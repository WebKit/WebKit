#!/usr/bin/env python3
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n\r\n')

sys.stdout.write(
    '''<p>Test that null values in XHR login/password parameters are treated correctly.</p>
<p>No auth tokens should be sent with this request.</p>
<pre id='syncResult'> </pre>
<script>
if (window.testRunner)
  testRunner.dumpAsText();

req = new XMLHttpRequest;
req.open('POST', '{}', false, null, null);
req.send();
document.getElementById('syncResult').firstChild.nodeValue = req.responseText;
</script>
'''.format(
    'http://foo:bar@{}/xmlhttprequest/resources/echo-auth.py'.format(os.environ.get('HTTP_HOST', '?')),
))
