<?php

header('HTTP/1.1 302 Moved Temporarily');
header('Content-Type: text/html; charset=utf-8');
header('Cache-Control: private, s-maxage=0, max-age=0, must-revalidate');
header('Expires: Thu, 01 Jan 1970 00:00:00 GMT');
header('Vary: Accept-Encoding, Cookie');
header('Connection: keep-alive');
header('Location: ' . rand(0, 9) . '.php');

?>
