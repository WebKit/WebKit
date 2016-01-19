<?php
 $request_origin_value = $_SERVER["HTTP_ORIGIN"];
 if (!is_null($request_origin_value)) {
     header("Access-Control-Allow-Origin: null");
     echo "PASS";
 }
?>
