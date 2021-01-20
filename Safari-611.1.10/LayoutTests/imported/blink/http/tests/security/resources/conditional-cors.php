<?php
if (isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])) {
   header('HTTP/1.0 304 Not Modified');
   exit;
}
header('Access-Control-Allow-Origin: *');
header('Last-Modified: ' . gmdate("D, d M Y H:i:s", filemtime("conditional-cors.php")) . " GMT");
header('Content-Type: application/javascript');
echo '2;'
?>
