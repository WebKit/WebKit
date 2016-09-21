<?php
    if($_SERVER["HTTP_ACCEPT"] == "image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5")
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
