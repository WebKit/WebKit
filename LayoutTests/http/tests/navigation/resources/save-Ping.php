<?php
require_once 'ping-file-path.php';

$pingFile = fopen($pingFilePath . ".tmp", 'w');
$httpHeaders = $_SERVER;
ksort($httpHeaders, SORT_STRING);
foreach ($httpHeaders as $name => $value) {
    if ($name === "CONTENT_TYPE" || $name === "HTTP_REFERER" || $name === "HTTP_PING_TO" || $name === "HTTP_PING_FROM"
        || $name === "REQUEST_METHOD" || $name === "REQUEST_URI" || $name === "HTTP_HOST" || $name === "HTTP_COOKIE")
        fwrite($pingFile, "$name: $value\n");
}
fclose($pingFile);
rename($pingFilePath . ".tmp", $pingFilePath);

if (!isset($DO_NOT_CLEAR_COOKIES) || !$DO_NOT_CLEAR_COOKIES) {
    foreach ($_COOKIE as $name => $value)
        setcookie($name, "deleted", time() - 60, "/");
}
?>
