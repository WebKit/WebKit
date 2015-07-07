<?php
  header('HTTP/1.0 200 OK');
  header('Content-type: text/html');
  header('Refresh: 0;URL=javascript:window.top.location="about:blank"');
?>
<!DOCTYPE html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
PASS: This is the content that appears in place of a refresh.
</body>
