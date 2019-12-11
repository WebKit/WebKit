<?php
  header('HTTP/1.0 200 OK');
  header('Content-type: text/html');
  header('Refresh: 0;URL=%6A%61%76%61%73%63%72%69%70%74:window.top.location="about:blank"');
?>
<!DOCTYPE html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
PASS: This is the content that appears in place of a failed refresh.
</body>
