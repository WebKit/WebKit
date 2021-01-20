<?php
header("Content-Security-Policy-Report-Only: script-src 'self'");
?>
<!DOCTYPE html>
<html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();

eval("alert('PASS: eval() executed as expected.');");
</script>
</body>
</html>
