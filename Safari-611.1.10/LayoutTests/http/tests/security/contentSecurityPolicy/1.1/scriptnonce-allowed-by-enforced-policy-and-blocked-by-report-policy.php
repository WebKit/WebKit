<?php
    header("Content-Security-Policy-Report-Only: script-src 'nonce-that-is-not-equal-to-dummy' 'nonce-dump-as-text'; report-uri ../resources/save-report.php?test=/security/contentSecurityPolicy/1.1/scriptnonce-allowed-by-enforced-policy-and-blocked-by-report-policy.php");
    header("Content-Security-Policy: script-src 'nonce-dummy' 'nonce-dump-as-text'");
?>
<!DOCTYPE html>
<html>
<head>
<script nonce="dump-as-text">
if (window.testRunner)
    testRunner.dumpAsText();
</script>
</head>
<body>
<p id="result">FAIL did not execute script.</p>
<script nonce="dummy">
document.getElementById("result").textContent = "PASS did execute script.";
</script>
<!-- FIXME: Call testRunner.dumpChildFramesAsText() and load
../resources/echo-report.php?test=/security/contentSecurityPolicy/1.1/scriptnonce-allowed-by-enforced-policy-and-blocked-by-report-policy.php
in an <iframe> once we fix reporting of nonce violations for report-only policies. See <https://bugs.webkit.org/show_bug.cgi?id=159830>. -->
</body>
</html>
