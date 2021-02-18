<?php
require_once "tokenSigningFilePath.php";

$tokenSigningFile = fopen($tokenSigningFilePath . ".tmp", 'w');
$httpHeaders = $_SERVER;
$cookiesFound = false;

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
}

if (!$cookiesFound) {
    fwrite($tokenSigningFile, "No cookies in token signing request.\n");
}

$requestBody = file_get_contents("php://input");
fwrite($tokenSigningFile, "Request body:\n$requestBody\n");

fclose($tokenSigningFile);
rename($tokenSigningFilePath . ".tmp", $tokenSigningFilePath);

header("HTTP/1.1 201 Created");
setcookie("cookieSetInTokenSigningResponse", "1", 0, "/");

?>
