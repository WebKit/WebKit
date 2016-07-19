<?php
    header("Content-Security-Policy-Report-Only: script-src http://example.com 'unsafe-inline'; report-uri ../resources/save-report.php?test=script-blocked-sends-multiple-reports-report-only");
    header("Content-Security-Policy: script-src http://127.0.0.1:8000 'unsafe-inline'; report-uri ../resources/save-report.php?test=script-blocked-sends-multiple-reports-enforced-1" .
           ", script-src http://127.0.0.1:8000 https://127.0.0.1:8443 'unsafe-inline'; report-uri ../resources/save-report.php?test=script-blocked-sends-multiple-reports-enforced-2");
?>
<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
}
</script>
</head>
<body>
<!-- Trigger CSP violation -->
<script src="http://localhost:8000/security/contentSecurityPolicy/resources/alert-fail.js"></script>
<!-- Reports -->
<iframe name="report-only" src="../resources/echo-report.php?test=script-blocked-sends-multiple-reports-report-only"></iframe>
<iframe name="enforced-1" src="../resources/echo-report.php?test=script-blocked-sends-multiple-reports-enforced-1"></iframe>
<iframe name="enforced-2" src="../resources/echo-report.php?test=script-blocked-sends-multiple-reports-enforced-2"></iframe>
</body>
</html>
