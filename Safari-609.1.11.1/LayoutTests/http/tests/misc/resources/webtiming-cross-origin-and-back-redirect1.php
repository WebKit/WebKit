<?php
  // Tests run from 127.0.0.1, so localhost will appear to be a different origin.
  header('Location: http://localhost:8000/misc/resources/webtiming-cross-origin-and-back-redirect2.php');
  header('HTTP/1.0 302 Found');
?>
