<?php
    header("Content-Security-Policy-Report-Only: connect-src http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.php");
?>
<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
</head>
<body>
<pre id="console"></pre>
<script>
function log(msg)
{
    document.getElementById("console").appendChild(document.createTextNode(msg + "\n"));
}

try {
    navigator.sendBeacon("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.php?url=http://localhost:8000/security/contentSecurityPolicy/resources/echo-report.php");
    log("Pass");
} catch(e) {
    log("Fail");
}
</script>
</body>
</html>
