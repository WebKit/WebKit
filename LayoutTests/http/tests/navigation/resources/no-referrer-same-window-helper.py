#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Cache-Control: no-store, private, max-age=0\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<html><body>
The Referrer displayed below should be empty.<br/>
<div id="referrer">Referrer: {}</div>
<div id="console"></div>
<script>
    var line = document.createElement('div');
    line.appendChild(document.createTextNode(document.getElementById("referrer").innerText == "Referrer:" ? "PASS" : "FAIL"));
    document.getElementById('console').appendChild(line);

    if (window.testRunner)
        testRunner.notifyDone();
</script>
</body></html>'''.format(os.environ.get('HTTP_REFERER', '')))
