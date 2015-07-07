<?php
    $mimetype = $_GET["mimetype"];
    $filename = $_GET["filename"];
    $charset = $_GET["charset"];

    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-store, no-cache, must-revalidate");
    header("Pragma: no-cache");
    if ($charset)
        header("Content-Type: " . $mimetype . "; charset=" . $charset);
    else
        header("Content-Type: " . $mimetype);

    readfile($filename);
?>
