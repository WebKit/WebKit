<?php
    header("Content-Security-Policy-Report-Only: script-src 'nonce-dummy' 'nonce-dump-as-text'");
    header("Content-Security-Policy: script-src 'nonce-that-is-not-equal-to-dummy' 'nonce-dump-as-text'; report-uri ../resources/save-report.php?test=/security/contentSecurityPolicy/1.1/scriptnonce-blocked-by-enforced-policy-and-allowed-by-report-policy.php");
?>
<!DOCTYPE html>
<html>
<head>
<script nonce="dump-as-text">
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
}
</script>
</head>
<body>
<p id="result">PASS did not execute script.</p>
<script nonce="dummy">
document.getElementById("result").textContent = "FAIL did execute script.";
</script>
<iframe src="../resources/echo-report.php?test=/security/contentSecurityPolicy/1.1/scriptnonce-blocked-by-enforced-policy-and-allowed-by-report-policy.php"></iframe>
</body>
</html>
