<?php
require_once "tokenSigningFilePath.php";

$tokenSigningFile = fopen($tokenSigningFilePath . ".tmp", 'a');
$httpHeaders = $_SERVER;
$cookiesFound = false;
// This php will respond to two consecutive server requests.
// It will only complete the transaction when the second request finishes.
$isSecondRequest = false;

if ($value = $httpHeaders["REQUEST_METHOD"]) {
    fwrite($tokenSigningFile, "REQUEST_METHOD: $value\n");
}

if ($value = $httpHeaders["HTTP_HOST"]) {
    fwrite($tokenSigningFile, "HTTP_HOST: $value\n");
}

if ($value = $httpHeaders["HTTP_COOKIE"]) {
    fwrite($tokenSigningFile, "Cookies in token signing request: $value\n");
    $cookiesFound = true;
}

if ($value = $httpHeaders["CONTENT_TYPE"]) {
    fwrite($tokenSigningFile, "Content type: $value\n");
}

if ($value = $httpHeaders["REQUEST_URI"]) {
    $value = $httpHeaders["REQUEST_URI"];
    $positionOfDummy = strpos($value, "?dummy=");
    if ($positionOfDummy === false)
        $outputURL = $value;
    else
        $outputURL = substr($value, 0, $positionOfDummy);
    fwrite($tokenSigningFile, "REQUEST_URI: $outputURL\n");

    $positionOfDummy = strpos($value, "&second=");
    if ($positionOfDummy != false)
        $isSecondRequest = true;
}

if (!$cookiesFound) {
    fwrite($tokenSigningFile, "No cookies in token signing request.\n");
}

$requestBody = file_get_contents("php://input");
fwrite($tokenSigningFile, "Request body:\n$requestBody\n");

fclose($tokenSigningFile);
// Complete the transaction.
if ($isSecondRequest)
    rename($tokenSigningFilePath . ".tmp", $tokenSigningFilePath);

header("HTTP/1.1 201 Created");
setcookie("cookieSetInTokenSigningResponse", "1", 0, "/");

?>
