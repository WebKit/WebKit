<?php
    $request_origin_value = $_SERVER["HTTP_ORIGIN"];

    if ($_SERVER['REQUEST_METHOD'] == "POST") {
        if (!is_null($request_origin_value))
            header("Access-Control-Allow-Origin: $request_origin_value");
        echo "PASS";
    } else {
        echo "FAIL";
    }
?>
