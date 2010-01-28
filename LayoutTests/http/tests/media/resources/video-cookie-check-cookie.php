<?php
    if($_COOKIE["TEST"])
    {
        $extension = substr($_COOKIE["TEST"], -3);

        if ($extension == 'mp4') {
               header("Content-Type: video/mp4");
               $fileName = "test.mp4";
        } else if ($extension == 'ogv') {
               header("Content-Type: video/ogg");
               $fileName = "test.ogv";
        } else
               die;

        header("Cache-Control: no-store");
        header("Connection: close");

        $fn = fopen($fileName, "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
?>
