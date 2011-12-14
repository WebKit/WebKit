<?php
  // Tests run from 127.0.0.1, so localhost will appear to be a different origin.
  header('Location: http://localhost:8080/misc/resources/webtiming-cross-origin-redirect.html');
  header('HTTP/1.0 302 Found');
?>
