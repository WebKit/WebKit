<?php
    header("Content-Security-Policy: sandbox");
?>
<!DOCTYPE html>
<html>
<body>
<!-- This test passes if it doesn't alert FAIL. -->
<iframe src="data:text/html,<script>alert('FAIL');</script>" width="100" height="100"></iframe>
</body>
</html>
