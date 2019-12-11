<?php

header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Credentials: true");

if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
    header("Access-Control-Allow-Methods: GET");
    header("Access-Control-Allow-Headers: origin, accept-encoding, referer, range");
    exit;
}

@include("../../media/resources/serve-video.php");

?>
