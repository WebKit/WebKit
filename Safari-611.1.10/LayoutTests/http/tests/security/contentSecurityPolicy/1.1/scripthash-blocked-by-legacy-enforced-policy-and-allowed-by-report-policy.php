<?php
    header("Content-Security-Policy-Report-Only: script-src 'sha256-AJqUvsXuHfMNXALcBPVqeiKkFk8OLvn3U7ksHP/QQ90=' 'nonce-dump-as-text'");
    header("X-WebKit-CSP: script-src 'nonce-dump-as-text'; report-uri ../resources/save-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-blocked-by-legacy-enforced-policy-and-allowed-by-report-policy.php");
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
<script>
document.getElementById("result").textContent = "PASS did execute script.";
</script>
<!-- Call testRunner.dumpChildFramesAsText() and load
<iframe src="../resources/echo-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-blocked-by-legacy-enforced-policy-and-allowed-by-report-policy.php"></iframe>
once we fix reporting of nonce violations for report-only policies. See <https://bugs.webkit.org/show_bug.cgi?id=159830>. -->
</body>
</html>
