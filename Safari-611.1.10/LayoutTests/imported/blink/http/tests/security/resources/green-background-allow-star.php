<?php
header("Access-Control-Allow-Origin: *");

$name = 'green-background.css';
$fp = fopen($name, 'rb');
header("Content-Type: text/css");
header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
