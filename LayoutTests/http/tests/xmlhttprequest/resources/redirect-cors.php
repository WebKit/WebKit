<?php
    $url = $_GET['url'];
    $redirect_preflight = $_GET['redirect-preflight'];
    $access_control_allow_origin = $_GET['access-control-allow-origin'];
    $access_control_allow_credentials = $_GET['access-control-allow-credentials'];
    $access_control_allow_headers = $_GET['access-control-allow-headers'];

    if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
        if ($redirect_preflight == "true") {
            header("HTTP/1.1 302");
            header("Location: $url");
        }
        else {
            header("HTTP/1.1 200");
        }
        header("Access-Control-Allow-Methods: GET");
        header("Access-Control-Max-Age: 1");
    } else if ($_SERVER['REQUEST_METHOD'] == "GET") {
        header("HTTP/1.1 302");
        header("Location: $url");
    }
    if (!is_null($access_control_allow_origin))
        header("Access-Control-Allow-Origin: $access_control_allow_origin");
    if (!is_null($access_control_allow_credentials))
        header("Access-Control-Allow-Credentials: $access_control_allow_credentials");
    if (!is_null($access_control_allow_headers))
        header("Access-Control-Allow-Headers: $access_control_allow_headers");
?>
