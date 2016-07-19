<?php
    header("Content-Security-Policy-Report-Only: object-src 'none'"); // Arbitrary non-{script, img}-src directive
    header("Content-Security-Policy: script-src 'sha256-+RQEi26Zq9zK8V7JRS2gLaVGlBEZ+fL8HA0f7zo5jUk=', img-src 'none'"); // Two policies; second policy has arbitrary non-script-src directive
?>
<!DOCTYPE html>
<html>
<body>
<p id="result">FAIL did not execute script.</p>
<script>document.getElementById("result").textContent = "PASS did execute script.";</script>
</body>
</html>
