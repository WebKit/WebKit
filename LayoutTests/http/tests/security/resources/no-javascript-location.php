<?php
  header('HTTP/1.0 200 OK');
  header('Content-type: text/html');
  header('Location: javascript:window.top.location="about:blank"');
?>
<!DOCTYPE html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
FAIL: This content should not appear after a failed Location redirect to a javascript: URL.
</body>
