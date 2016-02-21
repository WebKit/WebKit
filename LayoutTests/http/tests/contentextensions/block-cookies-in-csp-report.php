<?php
    header("Content-Security-Policy: img-src 'self'; report-uri /contentextensions/resources/save-ping.php?test=contentextensions-block-cookies-in-csp-report");
?>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}

function deletePing() {
    var deletePingContainer = document.getElementById("delete_ping_container");
    deletePingContainer.innerHTML = '<img src="resources/delete-ping.php?test=contentextensions-block-cookies-in-csp-report" onerror="loadCrossDomainImage();">';
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
    iframe.src = "resources/get-ping-data.php?test=contentextensions-block-cookies-in-csp-report";
}
</script>
</head>

<body>
This test creates a CSP violation report, but the report URL matches a 'block-cookie' rule.
<img src="/cookies/resources/cookie-utility.php?queryfunction=setFooCookie"
    onerror="deletePing();">
<div id="delete_ping_container"></div>
<iframe id="result_frame" name="result_frame"><!-- Will contain ping data received by server --></iframe>
</body>

