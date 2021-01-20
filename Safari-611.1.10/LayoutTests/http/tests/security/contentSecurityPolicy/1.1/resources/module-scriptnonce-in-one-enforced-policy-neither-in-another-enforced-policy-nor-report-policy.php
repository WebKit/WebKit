<?php
    header("Content-Security-Policy-Report-Only: object-src 'none'"); // Arbitrary non-{script, img}-src directive
    header("Content-Security-Policy: script-src 'nonce-test', img-src 'none'"); // Two policies; second policy has arbitrary non-script-src directive
?>
<!DOCTYPE html>
<html>
<body>
<p id="result">FAIL did not execute script.</p>
<script type="module" nonce="test">
document.getElementById("result").textContent = "PASS did execute script.";
if (window.testRunner)
    testRunner.notifyDone();
</script>
</body>
</html>
