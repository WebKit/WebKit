<?php
require_once 'conversionFilePath.php';

$conversionFile = fopen($conversionFilePath . ".tmp", 'w');
$httpHeaders = $_SERVER;
$cookiesFound = false;

if ($value = $httpHeaders["HTTP_HOST"]) {
    fwrite($conversionFile, "HTTP_HOST: $value\n");
}

if ($value = $httpHeaders["HTTP_COOKIE"]) {
    fwrite($conversionFile, "Cookies in attribution request: $value\n");
    $cookiesFound = true;
}

if ($value = $httpHeaders["CONTENT_TYPE"]) {
    fwrite($conversionFile, "Content type: $value\n");
}

if ($value = $httpHeaders["REQUEST_URI"]) {
    $value = $httpHeaders["REQUEST_URI"];
    $positionOfNonce = strpos($value, "?nonce=");
    if ($positionOfNonce === false)
        $outputURL = $value;
    else
        $outputURL = substr($value, 0, $positionOfNonce);
    fwrite($conversionFile, "REQUEST_URI: $outputURL\n");
}

if (!$cookiesFound) {
    fwrite($conversionFile, "No cookies in attribution request.\n");
}

$requestBody = file_get_contents('php://input');
fwrite($conversionFile, "Request body:\n$requestBody\n");

fclose($conversionFile);
rename($conversionFilePath . ".tmp", $conversionFilePath);

header("HTTP/1.1 200 OK");
setcookie("cookieSetInConversionReport", "1", 0, "/");

?>
