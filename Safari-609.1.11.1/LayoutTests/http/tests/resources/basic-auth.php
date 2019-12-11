<?php
  if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_REQUEST['uid']) || ($_REQUEST['uid'] != $_SERVER['PHP_AUTH_USER'])) {
   header('WWW-Authenticate: Basic realm="WebKit Test Realm"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
  } else {
   echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
  }
?>
