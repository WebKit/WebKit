<?php

$delay = $_GET['delay'];
if (isset($delay))
    usleep($delay);

$origin = $_GET['origin'];
if (isset($origin))
    header("Access-Control-Allow-Origin: " . $origin);
else if ($_SERVER["HTTP_ORIGIN"]) {
    header("Access-Control-Allow-Origin: " . $_SERVER["HTTP_ORIGIN"]);
    header("Vary: Origin");
}

$allowCredentials = $_GET['allowCredentials'];
if (isset($allowCredentials))
    header("Access-Control-Allow-Credentials: true");

$allowCache = $_GET['allowCache'];
if (isset($allowCache))
    header("Cache-Control: max-age=100");

$name = $_GET['name'];
if (!isset($name))
    $name = 'abe.png';

$fp = fopen($name, 'rb');

$contentType = $_GET['contentType'];
if (!isset($contentType))
    $contentType = 'image/png';

header("Content-Type: " . $contentType);

header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
