<?php
    header("Content-Security-Policy: img-src 'self'; report-uri http://localhost:8000/contentextensions/resources/save-ping.php?test=contentextensions-block-csp-report");
?>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
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
    iframe.src = "resources/get-ping-data.php?test=contentextensions-block-csp-report&timeout_ms=1000";
    // Why timeout_ms=1000:
    // To pass the test, the ping shouldn't arrive, so we need to
    // timeout at some point. We don't have to wait too long because
    // the console message can tell us whether the ping was blocked.
}
</script>
</head>

<body>
This test creates a CSP violation report, but the report URL matches a 'block' rule.
<img src="resources/delete-ping.php?test=contentextensions-block-csp-report" onerror="loadCrossDomainImage();">
<iframe id="result_frame" name="result_frame"><!-- Will contain ping data received by server --></iframe>
</body>

