<?php
    header("Content-Security-Policy: sandbox allow-scripts");
?>
<!DOCTYPE html>
<html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
This test passes if it does alert pass.
<script>
alert('PASS');
</script>
</body>
</html>
