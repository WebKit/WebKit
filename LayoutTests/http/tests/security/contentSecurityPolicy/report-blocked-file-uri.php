<?php
    header("Content-Security-Policy: img-src 'none'; report-uri resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
    <script>
        testRunner.addOriginAccessAllowListEntry('http://127.0.0.1:8000', 'file', '', true);
        var localImageLocation = testRunner.pathToLocalResource('file:///tmp/LayoutTests/http/tests/security/resources/compass.jpg');

        var localImageElement = document.createElement('img');
        localImageElement.src = localImageLocation;
        document.body.appendChild(localImageElement);
    </script>
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
