<?php
$pingFile = fopen("ping.txt.tmp", 'w');
$httpHeaders = $_SERVER;
ksort($httpHeaders, SORT_STRING);
foreach ($httpHeaders as $name => $value) {
    if ($name === "CONTENT_TYPE" || $name === "HTTP_REFERER" || $name === "HTTP_PING_TO" || $name === "HTTP_PING_FROM" || $name === "REQUEST_METHOD" || $name === "HTTP_COOKIE")
        fwrite($pingFile, "$name: $value\n");
}
fclose($pingFile);
rename("ping.txt.tmp", "ping.txt");
foreach ($_COOKIE as $name => $value)
    setcookie($name, "deleted", time() - 60, "/");
?>
