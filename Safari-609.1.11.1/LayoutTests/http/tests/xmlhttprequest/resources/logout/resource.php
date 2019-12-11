<?php
  header("Cache-Control: no-cache, no-store");

  if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_SERVER['PHP_AUTH_PW']) || $_SERVER['PHP_AUTH_USER'] != 'user' || $_SERVER['PHP_AUTH_PW'] != 'pass') {
   header('WWW-Authenticate: Basic realm="Please cancel this authentication dialog"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
  } else {
   echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
  }
?>
