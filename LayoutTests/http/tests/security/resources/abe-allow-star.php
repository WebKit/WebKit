<?php
header("Access-Control-Allow-Origin: *");

$allowCache = $_GET['allowCache'];
if (isset($allowCache))
    header("Cache-Control: max-age=100");

$name = 'abe.png';
$fp = fopen($name, 'rb');
header("Content-Type: image/png");
header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
