<?php
    header("Content-Security-Policy-Report-Only: img-src 'none'; report-uri resources/save-report.php");
?>
The origin of this image should show up in the violation report.
<img src="http://localhost:8080/security/resources/abe.png">
<script src="resources/go-to-echo-report.js"></script>
