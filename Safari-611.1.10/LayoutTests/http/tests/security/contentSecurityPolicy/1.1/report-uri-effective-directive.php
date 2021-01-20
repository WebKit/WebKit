<?php
    header("Content-Security-Policy: default-src 'self'; report-uri ../resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
    <script>
        // This script block will trigger a violation report.
        alert('FAIL');
    </script>
    <script src="../resources/go-to-echo-report.js"></script>
</body>
</html>
