<?php

header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET");
header("Access-Control-Allow-Headers: authorization, x-webkit, content-type");

if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
    header("HTTP/1.1 200");
    return;
}

$redirect = $_GET['redirect'];
$url = $_GET['url'];

if (isset($redirect)) {
    header("HTTP/1.1 302");
    header("Location: $url");
    return;
}

header("HTTP/1.1 200");

if (!isset($_SERVER["HTTP_X_WEBKIT"])) {
    echo "not found any x-webkit header found";
} else {
    echo "x-webkit header found: " . $_SERVER["HTTP_X_WEBKIT"];
}
echo "\n";
if (!isset($_SERVER["HTTP_ACCEPT"])) {
    echo "not found any accept header found";
} else {
    echo "accept header found: " . $_SERVER["HTTP_ACCEPT"];
}
echo "\n";
if (!isset($_SERVER["CONTENT_TYPE"])) {
    echo "not found any content-type header";
} else {
    echo "content-type header found: " . $_SERVER["CONTENT_TYPE"];
}
echo "\n";
if (!isset($_SERVER['PHP_AUTH_USER'])) {
    echo "not found any authorization header";
} else {
    echo "authorization header found";
}

?>
