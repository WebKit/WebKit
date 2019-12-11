<?php
    header("Content-Security-Policy-Report-Only: script-src 'sha256-33badf00d3badf00d3badf00d3badf00d3badf00d33=' 'nonce-dump-as-text'; report-uri ../resources/save-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-blocked-by-legacy-enforced-policy-and-blocked-by-report-policy2.php");
?>
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="X-WebKit-CSP" content="script-src 'nonce-dump-as-text'">
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
<iframe src="../resources/echo-report.php?test=/security/contentSecurityPolicy/1.1/scripthash-blocked-by-legacy-enforced-policy-and-blocked-by-report-policy2.php"></iframe>
</body>
</html>
