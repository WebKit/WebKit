<?php
  if (!isset($_SERVER['PHP_AUTH_USER'])) {
   header('HTTP/1.0 500 Internal Server Error');
   echo 'Where is my default auth?';
   exit;
  } else {
   echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
  }
?>
