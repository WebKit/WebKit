#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: connect-src http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.py\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<!DOCTYPE html>
<html>
<head>
    <script src="/js-test-resources/js-test-pre.js"></script>
</head>
<body>
    <script>
        window.jsTestIsAsync = true;
        function log(msg) {
            document.getElementById("console").appendChild(document.createTextNode(msg + "\\n"));
        }

        var xhr = new XMLHttpRequest;
        try {
            // Redirect to a different host, because as of CSP2 paths are ignored when matching after a redirect.
            xhr.open("GET", "resources/redir.py?url=http://localhost:8000/security/contentSecurityPolicy/resources/xhr-redirect-not-allowed.py", true);
        } catch(e) {
            testFailed("XMLHttpRequest.open() should not throw an exception.");
        }

        xhr.onload = function () {
            testPassed("XMLHttpRequest.send() did follow the redirect.");
            finishJSTest();
        };

        xhr.onerror = function () {
            testFailed("XMLHttpRequest.send() did not follow the redirect.");
            finishJSTest();
        };

        xhr.send();
    </script>
</script>
<script src="/js-test-resources/js-test-post.js"></script>
</body>
</html>''')
