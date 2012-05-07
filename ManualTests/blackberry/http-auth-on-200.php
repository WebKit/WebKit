<?php
  header('WWW-Authenticate: Basic realm="FAIL"');
  header('HTTP/1.0 200 OK');
  header('Content-Type: text/plain');
  echo 'PASS if you did not see an authentication dialog';
?>
