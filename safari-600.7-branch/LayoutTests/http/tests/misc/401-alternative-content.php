<?php
  if (!isset($_SERVER['PHP_AUTH_USER'])) {
   header('WWW-Authenticate: Basic realm="Please press Cancel"');
   header('HTTP/1.0 401 Unauthorized');
   header('Content-Type: text/html');
   echo '<script>';
   echo '   if (window.testRunner)';
   echo '       testRunner.dumpAsText();';
   echo '</script>';
   echo 'PASS';
   exit;
  } else {
   echo "FAIL: Why do you have credentials?";
  }
?>
