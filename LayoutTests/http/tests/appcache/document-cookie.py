#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Set-Cookie: foo=bar; path=/\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<html manifest="resources/document-cookie.manifest">

<div>This tests that cookies set on the main document are used when accessing resources in the manifest.</div>
<div id="result">Not run yet</div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText()
    testRunner.waitUntilDone();
}

function dynamicScriptLoad() {
    var script = document.createElement("script");
    script.type = "text/javascript";
    script.src = "./resources/cookie-protected-script.py"; 
    document.getElementsByTagName("head")[0].appendChild(script);
}

function cached()
{
    setTimeout("dynamicScriptLoad();", 0);
}

applicationCache.addEventListener('cached', cached, false);
</script>
</html>''')