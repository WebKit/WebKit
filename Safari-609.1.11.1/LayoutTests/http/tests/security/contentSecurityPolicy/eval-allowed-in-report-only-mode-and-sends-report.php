<?php
header("Content-Security-Policy-Report-Only: script-src 'self' 'unsafe-inline'; report-uri resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<body>
<script>
try {
    eval("alert('PASS: eval() allowed!')");
} catch (e) {
    console.log("FAIL: eval() blocked!");
}
</script>
<script src="resources/go-to-echo-report.js"></script>
</body>
</html>
