<?php
    header("Content-Type: text/javascript");
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT\n");
    header("Cache-Control: no-store, no-cache, must-revalidate\n");
    header("Pragma: no-cache\n");
    echo "document.write('" . $_SERVER['REQUEST_URI'] . "');";
?>
