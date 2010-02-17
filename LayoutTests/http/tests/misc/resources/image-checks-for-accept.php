<?php
    if($_SERVER["HTTP_ACCEPT"] == "*/*" || $_SERVER["HTTP_ACCEPT"] == "image/*" || $_SERVER["HTTP_ACCEPT"] == "image/jpg")
    {
        header("Content-Type: image/jpg");
        header("Cache-Control: no-store");
        header("Connection: close");

        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
?>
