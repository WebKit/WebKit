#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'self\'; report-uri /contentextensions/resources/save-ping.py?test=contentextensions-block-cookies-in-csp-report\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}

function deletePing() {
    var deletePingContainer = document.getElementById("delete_ping_container");
    deletePingContainer.innerHTML = '<img src="resources/delete-ping.py?test=contentextensions-block-cookies-in-csp-report" onerror="loadCrossDomainImage();">';
}

function loadCrossDomainImage() {
    // Trying to load an image from a different port
    // will result in a CSP violation.
    var img = new Image(1, 1);
    img.src = "http://localhost/foo.png";
    showPingResult();
}

function showPingResult() {
    var iframe = document.getElementById("result_frame");
    iframe.onload = function() {
        if (window.testRunner) { testRunner.notifyDone(); }
    }
    iframe.src = "resources/get-ping-data.py?test=contentextensions-block-cookies-in-csp-report";
}
</script>
</head>

<body>
This test creates a CSP violation report, but the report URL matches a 'block-cookie' rule.
<img src="/cookies/resources/cookie-utility.py?queryfunction=setFooCookie"
    onerror="deletePing();">
<div id="delete_ping_container"></div>
<iframe id="result_frame" name="result_frame"><!-- Will contain ping data received by server --></iframe>
</body>''')