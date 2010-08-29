<?php
header("X-XSS-Protection: full-block");
?>
<!DOCTYPE html>
<html>
<head>
<script src="http://127.0.0.1:8000/security/xssAuditor/resources/utilities.js"></script>
<script>
if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.dumpChildFramesAsText();
    layoutTestController.waitUntilDone();
    layoutTestController.setXSSAuditorEnabled(true);
}
function checkIfDone()
{
    checkIfFrameLocationMatchesURLAndCallDone('frame', document.getElementById('frame').src);
}
</script>
</head>
<body>
<p>This tests that the header X-XSS-Protection is not inherited by the iframe below:</p>
<iframe id="frame" onload="checkIfDone()" src="http://127.0.0.1:8000/security/xssAuditor/resources/echo-intertag.pl?q=<script>alert(/XSS/)</script><p>If you see this message and no JavaScript alert() then the test PASSED.</p>">
</iframe>
</body>
</html>
