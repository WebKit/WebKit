<?php
    header("Content-Security-Policy-Report-Only: script-src 'sha256-AJqUvsXuHfMNXALcBPVqeiKkFk8OLvn3U7ksHP/QQ90=' 'nonce-dump-as-text'");
    header("Content-Security-Policy: script-src 'sha256-33badf00d3badf00d3badf00d3badf00d3badf00d33=' 'nonce-dump-as-text'; report-uri ../resources/save-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-blocked-by-enforced-policy-and-allowed-by-report-policy.php");
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
<script>
document.getElementById("result").textContent = "FAIL did execute script.";
</script>
<iframe src="../resources/echo-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-blocked-by-enforced-policy-and-allowed-by-report-policy.php"></iframe>
</body>
</html>
