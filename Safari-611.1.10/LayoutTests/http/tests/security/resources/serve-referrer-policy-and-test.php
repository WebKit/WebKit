<?php
$isTestingMultipart = $_GET["isTestingMultipart"];
$destinationOrigin = $_GET["destinationOrigin"];

header('HTTP/1.0 200 OK');
header('Referrer-Policy: ' . $_GET["value"]);
if ($isTestingMultipart) {
    header("Content-Type: multipart/x-mixed-replace;boundary=boundary");
    echo("--boundary\r\n");
    echo("Referrer-Policy: " . $_GET["value"] . "\r\n");
    echo("Content-type: text/html\r\n");
    echo("\r\n");
    echo("<iframe src='" . $destinationOrigin . "security/resources/postReferrer.php'></iframe>\r\n");
    echo("--boundary\r\n");
} else {
    header("Content-Type: text/html");
    echo("\r\n");
    echo("<iframe src='" . $destinationOrigin . "security/resources/postReferrer.php'></iframe>\r\n");
}
?>
