#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'none\'; report-uri http://localhost:8080/security/contentSecurityPolicy/resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html>
<meta name="referrer" content="unsafe-url">
<body>
<script>
    // Normal browsing mode
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "http://localhost:8080/cookies/resources/setCookies.cgi", false);
    xhr.setRequestHeader("SET-COOKIE", "hello=world;path=/");
    xhr.send(null);

    if (window.testRunner)
        testRunner.setPrivateBrowsingEnabled_DEPRECATED(true);
</script>

<!-- This image will generate a CSP violation report. -->
<img src="/security/resources/abe.png">

<script src="resources/go-to-echo-report.js"></script>
</body>
</html>''')
