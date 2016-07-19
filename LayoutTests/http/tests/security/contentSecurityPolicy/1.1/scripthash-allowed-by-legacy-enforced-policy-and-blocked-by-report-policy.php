<?php
    header("Content-Security-Policy-Report-Only: script-src 'sha256-33badf00d3badf00d3badf00d3badf00d3badf00d33=' 'nonce-dump-as-text'; report-uri ../resources/save-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-allowed-by-legacy-enforced-policy-and-blocked-by-report-policy.php");
    header("X-WebKit-CSP: script-src 'sha256-n7CDY/1Rg9w5XVqu2QuiqpjBw0MVHvwDmHpkLXsuu2g=' 'nonce-dump-as-text'");
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
<p id="result">FAIL did not execute script.</p>
<script>
document.getElementById("result").textContent = "PASS did execute script.";
</script>
<iframe src="../resources/echo-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-allowed-by-legacy-enforced-policy-and-blocked-by-report-policy.php"></iframe>
</body>
</html>
