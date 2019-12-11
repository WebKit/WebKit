<?php
 header("Access-Control-Allow-Origin: *");

 if ($_SERVER["REQUEST_METHOD"] == "OPTIONS") {
     if (stristr($_SERVER["HTTP_ACCESS_CONTROL_REQUEST_HEADERS"], "Origin") === false)
         header("Access-Control-Allow-Headers: X-Custom-Header");
 }
 else
     echo $_SERVER["HTTP_X_CUSTOM_HEADER"];
?>
