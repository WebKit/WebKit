<?php
    $request_origin_value = $_SERVER["HTTP_ORIGIN"];

    if ($_SERVER['REQUEST_METHOD'] == "GET") {
     header("HTTP/1.1 302");
     header("Location: http://localhost:8000/xmlhttprequest/resources/redirect-cors-origin-null-pass.php");
    }
    if (!is_null($request_origin_value)) {
        header("Access-Control-Allow-Origin: $request_origin_value");
    }
?>
