<?php
$cookieName = $_GET['cookie-name'];
$cookieValue = $_GET['cookie-value'];
$destination = $_GET['destination'];
header("Content-Type: text/html");
header("Cache-Control: no-store");
header("Set-Cookie: ${cookieName}=${cookieValue}; path=/");
$fp = fopen($destination, 'rb');
header("Content-Length: " . filesize($destination));

fpassthru($fp);
exit;
?>
