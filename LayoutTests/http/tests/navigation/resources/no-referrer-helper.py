#!/usr/bin/env python3

import os
import sys

sys.stdout.write(
    'Cache-Control: no-store, private, max-age=0\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<html><body>
<div id="console"></div>
<div id="referrer">
Referrer: {}
</div>
<br/>
<script>
function log(msg)
{{
    var line = document.createElement('div');
    line.appendChild(document.createTextNode(msg));
    document.getElementById('console').appendChild(line);
}}

    console.log(document.getElementById("referrer").innerText);
    console.log("window.opener: " + (window.opener ? window.opener.location : ""));

    if (window.testRunner)
        testRunner.notifyDone();
</script>

</body></html>'''.format(os.environ.get('HTTP_REFERER', '')))
