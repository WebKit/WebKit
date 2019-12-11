<?php
    header("Content-Security-Policy: img-src 'none'; report-uri resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
    <script>
        // This script block will trigger a violation report.
        var i = document.createElement('img');
        i.src = '/security/resources/abe.png';
        document.body.appendChild(i);
    </script>
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
