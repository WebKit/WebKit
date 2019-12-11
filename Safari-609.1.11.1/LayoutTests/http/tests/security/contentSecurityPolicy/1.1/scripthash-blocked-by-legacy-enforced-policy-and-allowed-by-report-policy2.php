<?php
    header("Content-Security-Policy-Report-Only: script-src 'sha256-AJqUvsXuHfMNXALcBPVqeiKkFk8OLvn3U7ksHP/QQ90=' 'nonce-dump-as-text'");
?>
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="X-WebKit-CSP" content="script-src 'nonce-dump-as-text'">
<script nonce="dump-as-text">
if (window.testRunner)
    testRunner.dumpAsText();
</script>
</head>
<body>
<p id="result">PASS did not execute script.</p>
<script>
document.getElementById("result").textContent = "FAIL did execute script.";
</script>
</body>
</html>
