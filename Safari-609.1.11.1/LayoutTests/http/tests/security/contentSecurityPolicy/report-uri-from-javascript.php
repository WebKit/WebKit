<?php
    header("Content-Security-Policy: img-src 'none'; report-uri resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
    <script src="resources/inject-image.js"></script>
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
