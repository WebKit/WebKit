<?php
  // 127.0.0.1 is where the test originally started. We redirected to "localhost" before this.
  header('Location: http://127.0.0.1:8000/misc/resources/webtiming-cross-origin-and-back2.html');
  header('HTTP/1.0 302 Found');
?>

