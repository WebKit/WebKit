<?
header("HTTP/1.1 404 Not Found");
header("Content-Length: 38616");
header("Content-Type: image/jpeg");
header("Last-Modified: Thu, 01 Jun 2006 06:09:43 GMT");
echo(file_get_contents("compass.jpg"));
?>
