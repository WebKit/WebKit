<?php
$meta_value = $_GET["meta_value"];

header('HTTP/1.0 200 OK');
header('Referrer-Policy: ' . $_GET["http_value"]);
header("Content-Type: text/html");
echo("\r\n");
echo("<!DOCTYPE html>\r\n");
echo("<html>\r\n");
echo("<head><meta name='referrer' content='" . $meta_value . "'></head>\r\n");
echo("<body>\r\n");
echo("<iframe src='postReferrer.php'></iframe>\r\n");
echo("</body>\r\n");
echo("</html>\r\n");
?>
