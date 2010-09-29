<?php
  if (!isset($_SERVER['PHP_AUTH_USER'])) {
   header('WWW-Authenticate: Basic realm="xhr basic-auth-default"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
  } else {
   echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
  }
?>
