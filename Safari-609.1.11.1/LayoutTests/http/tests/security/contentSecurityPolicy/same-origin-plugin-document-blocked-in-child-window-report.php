<?php
    header("Content-Security-Policy: script-src 'self' 'unsafe-inline'; plugin-types application/x-webkit-dummy-plugin; report-uri /security/contentSecurityPolicy/resources/save-report.php?test=/security/contentSecurityPolicy/same-origin-plugin-document-blocked-in-child-window-report.php");
?>
<!DOCTYPE html>
<html>
<head>
<script src="resources/checkDidSameOriginChildWindowLoad.js"></script>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.setCanOpenWindows();
    testRunner.setCloseRemainingWindowsWhenComplete(true);
    testRunner.waitUntilDone();
}
</script>
</head>
<body>
<script>
function navigateToCSPReport()
{
    window.location.href = "/security/contentSecurityPolicy/resources/echo-report.php?test=/security/contentSecurityPolicy/same-origin-plugin-document-blocked-in-child-window-report.php";
}

checkDidSameOriginChildWindowLoad(window.open("http://127.0.0.1:8000/plugins/resources/mock-plugin.pl"), navigateToCSPReport);
</script>
</body>
</html>
