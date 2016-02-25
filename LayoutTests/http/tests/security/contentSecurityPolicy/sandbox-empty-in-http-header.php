<?php
    header("Content-Security-Policy: sandbox");
?>
<!DOCTYPE html>
<html>
<body>
<!-- This test passes if it doesn't alert FAIL. -->
<script>alert('FAIL')</script>
</body>
</html>
