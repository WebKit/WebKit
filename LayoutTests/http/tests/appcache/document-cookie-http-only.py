#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Set-Cookie: scope=manifest; path=/appcache/resources/scope1; HttpOnly\r\n'
    'Set-Cookie: scope=script; path=/appcache/resources/scope2; HttpOnly\r\n'
    'Set-Cookie: foo=bar\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<html manifest="resources/scope1/cookie-protected-manifest.py">

<div>This tests that HttpOnly cookies set on the main document are used when accessing resources in the manifest.</div>
<div>This also tests that cookies used by appcache resource loading are scoped properly.</div>
<div id="log">Not checked cookie yet</div>
<div id="result1">Not run yet</div>
<div id="result">Not run yet</div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText()
    testRunner.waitUntilDone();
}

var cookieTest = document.cookie === "foo=bar" ? "PASSED" : "FAILED";
log.innerHTML = cookieTest + ": Some cookies should not be visible from JavaScript.";

function dynamicScriptLoad() {
    var script = document.createElement("script");
    script.type = "text/javascript";
    script.src = "./resources/scope2/cookie-protected-script.py";
    document.getElementsByTagName("head")[0].appendChild(script);

    setTimeout(() => {
        script = document.createElement("script");
        script.type = "text/javascript";
        script.src = "./resources/cookie-protected-script.py";
        document.getElementsByTagName("head")[0].appendChild(script);
    }, 0);
}

function cached()
{
    setTimeout("dynamicScriptLoad();", 0);
}

applicationCache.addEventListener('cached', cached, false);
</script>
</html>''')