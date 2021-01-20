<?php
  header("Cache-Control: no-cache, no-store");

  if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_SERVER['PHP_AUTH_PW']) ) {
   header('WWW-Authenticate: Basic realm="WebKit test re-login"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
  } else {
   echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
  }
?>
