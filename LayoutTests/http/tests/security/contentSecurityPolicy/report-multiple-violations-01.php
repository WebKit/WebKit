<?php
header("Content-Security-Policy-Report-Only: img-src 'none'; report-uri resources/does-not-exist");
?>
<!DOCTYPE html>
<html>
<body>
<p>This tests that multiple violations on a page trigger multiple reports.
The test passes if two PingLoader callbacks are visible in the output.</p>
<img src="../resources/abe.png">
<img src="../resources/eba.png">
</body>
</html>
