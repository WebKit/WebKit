<?php
  header('Content-Type: application/javascript');
?>
window.subresourceUserAgent = "<?php 
echo $_SERVER['HTTP_USER_AGENT'];
?>";
