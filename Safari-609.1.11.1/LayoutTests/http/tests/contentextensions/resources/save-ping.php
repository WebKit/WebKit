<?php
require_once 'ping-file-path.php';

$pingFile = fopen($pingFilePath . ".tmp", 'w');
$httpHeaders = $_SERVER;
$cookiesFound = false;
foreach ($httpHeaders as $name => $value) {
    if ($name === "HTTP_HOST" || $name === "REQUEST_URI")
        fwrite($pingFile, "$name: $value\n");
    else if ($name === "HTTP_COOKIE") {
        fwrite($pingFile, "Cookies in ping: $value\n");
        $cookiesFound = true;
    }
}
if (!$cookiesFound) {
    fwrite($pingFile, "No cookies in ping.\n");
}
fclose($pingFile);
rename($pingFilePath . ".tmp", $pingFilePath);

if (!isset($DO_NOT_CLEAR_COOKIES) || !$DO_NOT_CLEAR_COOKIES) {
    foreach ($_COOKIE as $name => $value)
        setcookie($name, "deleted", time() - 60, "/");
}
?>
