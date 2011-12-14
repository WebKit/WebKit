<?php
   header('WWW-Authenticate: Basic realm="WebKit Test Realm"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
?>
