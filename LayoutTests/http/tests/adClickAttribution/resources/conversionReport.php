<?php
require_once 'conversionFilePath.php';

$conversionFile = fopen($conversionFilePath . ".tmp", 'w');
$httpHeaders = $_SERVER;
$cookiesFound = false;
foreach ($httpHeaders as $name => $value) {
    if ($name === "HTTP_HOST") {
        fwrite($conversionFile, "$name: $value\n");
    } else if ($name === "REQUEST_URI") {
        $positionOfNonce = strpos($value, "&nonce=");
        if ($positionOfNonce === false)
            $outputURL = $value;
        else
            $outputURL = substr($value, 0, $positionOfNonce);
        fwrite($conversionFile, "$name: $outputURL\n");
    } else if ($name === "HTTP_COOKIE") {
        fwrite($conversionFile, "Cookies in conversion request: $value\n");
        $cookiesFound = true;
    }
}
if (!$cookiesFound) {
    fwrite($conversionFile, "No cookies in conversion request.\n");
}
fclose($conversionFile);
rename($conversionFilePath . ".tmp", $conversionFilePath);

header("HTTP/1.1 200 OK");
setcookie("cookieSetInConversionReport", "1", 0, "/");

?>
