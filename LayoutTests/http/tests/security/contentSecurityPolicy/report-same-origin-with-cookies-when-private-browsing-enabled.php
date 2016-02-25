<?php
    header("Content-Security-Policy: img-src 'none'; report-uri /security/contentSecurityPolicy/resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
<script>
    if (window.testRunner)
        testRunner.setPrivateBrowsingEnabled(true);

    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/cookies/resources/setCookies.cgi", false);
    xhr.setRequestHeader("SET-COOKIE", "hello=world;path=/");
    xhr.send(null);
</script>

<!-- This image will generate a CSP violation report. -->
<img src="/security/resources/abe.png">

<script src="resources/go-to-echo-report.js"></script>
</body>
</html>
