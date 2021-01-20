<?php
  if (!isset($_SERVER['PHP_AUTH_USER'])) {
   echo 'No authentication';
  } else {
   echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
  }
?>
