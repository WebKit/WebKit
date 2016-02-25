<?php
    header("Content-Security-Policy: img-src 'none'; report-uri http://localhost:8080/security/contentSecurityPolicy/resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
<script>
    // Normal browsing mode
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "http://localhost:8080/cookies/resources/setCookies.cgi", false);
    xhr.setRequestHeader("SET-COOKIE", "hello=world;path=/");
    xhr.send(null);

    if (window.testRunner)
        testRunner.setPrivateBrowsingEnabled(true);
</script>

<!-- This image will generate a CSP violation report. -->
<img src="/security/resources/abe.png">

<script src="resources/go-to-echo-report.js"></script>
</body>
</html>
