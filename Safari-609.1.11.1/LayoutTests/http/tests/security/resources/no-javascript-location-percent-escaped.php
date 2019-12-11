<?php
  header('HTTP/1.0 200 OK');
  header('Content-type: text/html');
  header('Location: %6A%61%76%61%73%63%72%69%70%74:window.top.location="about:blank"');
?>
<!DOCTYPE html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
FAIL: This content should not appear after a failed Location redirect to a javascript: URL.
</body>
