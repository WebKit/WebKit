<?php
header("Content-Security-Policy-Report-Only: img-src 'none'; report-uri resources/save-report-and-redirect-to-save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
<p>This test PASSED if the filename of the REQUEST_URI in the dumped report is save-report-and-redirect-to-save-report.php. Otherwise, it FAILED.</p>
<img src="../resources/abe.png"> <!-- Trigger CSP violation -->
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function navigateToReport()
{
    window.location = "/security/contentSecurityPolicy/resources/echo-report.php";
}

// We assume that if redirects were followed when saving the report that they will complete within one second.
// FIXME: Is there are better way to test that a redirect did not occur?
window.setTimeout(navigateToReport, 1000);
</script>
</body>
</html>
