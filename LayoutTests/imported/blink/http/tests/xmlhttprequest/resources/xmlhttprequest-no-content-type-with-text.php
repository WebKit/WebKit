<?php
    $request_origin_value = $_SERVER["HTTP_ORIGIN"];
    $request_content_type_value = $_SERVER["CONTENT_TYPE"];

    if (($_SERVER['REQUEST_METHOD'] == "POST") && ($request_content_type_value == "text/plain;charset=UTF-8")) {
        echo "PASS";
    } else {
        echo "FAIL";
        echo " Content-Type received $request_content_type_value";
    }
?>
