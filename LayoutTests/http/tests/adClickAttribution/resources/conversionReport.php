<?php
require_once 'conversionFilePath.php';

$conversionFile = fopen($conversionFilePath . ".tmp", 'w');
$httpHeaders = $_SERVER;
$cookiesFound = false;
foreach ($httpHeaders as $name => $value) {
    if ($name === "HTTP_HOST" || $name === "REQUEST_URI")
        fwrite($conversionFile, "$name: $value\n");
    else if ($name === "HTTP_COOKIE") {
        fwrite($conversionFile, "Cookies in conversion request: $value\n");
        $cookiesFound = true;
    }
}
if (!$cookiesFound) {
    fwrite($conversionFile, "No cookies in conversion request.\n");
}
fclose($conversionFile);
rename($conversionFilePath . ".tmp", $conversionFilePath);
?>
