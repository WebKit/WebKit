<?php
    $allimages = 'image/*';
    $pos = strpos($_SERVER["HTTP_ACCEPT"], $allimages);
    if ($pos !== false)
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
