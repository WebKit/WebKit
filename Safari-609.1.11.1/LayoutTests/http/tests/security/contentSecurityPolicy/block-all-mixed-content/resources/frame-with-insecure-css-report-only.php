<?php
    header("Content-Security-Policy-Report-Only: block-all-mixed-content; report-uri ../../resources/save-report.php?test=/security/contentSecurityPolicy/block-all-mixed-content/resources/frame-with-insecure-css-report-only.php");
?>
<!DOCTYPE html>
<html>
<head>
<style>
body {
    background-color: white;
}
</style>
<link rel="stylesheet" href="http://127.0.0.1:8000/security/mixedContent/resources/style.css">
</head>
<body>
This background color should be white.
<script>
    window.location.href = "/security/contentSecurityPolicy/resources/echo-report.php?test=/security/contentSecurityPolicy/block-all-mixed-content/resources/frame-with-insecure-css-report-only.php";
</script>
</body>
</html>
