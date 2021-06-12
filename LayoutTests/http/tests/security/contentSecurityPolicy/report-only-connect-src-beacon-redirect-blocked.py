#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: connect-src http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.py\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}
</script>
</head>
<body>
<pre id="console"></pre>
<script>
document.addEventListener('securitypolicyviolation', e => {
    testRunner.notifyDone();
});

function log(msg)
{
    document.getElementById("console").appendChild(document.createTextNode(msg + "\\n"));
}

try {
    navigator.sendBeacon("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.py?url=http://localhost:8000/security/contentSecurityPolicy/resources/echo-report.py");
    log("Pass");
} catch(e) {
    log("Fail");
}
</script>
</body>
</html>''')
