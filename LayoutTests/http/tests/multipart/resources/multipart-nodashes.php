<?php
    # Generates a multipart/x-mixed-replace response but doesn't
    # include the -- before the boundary.

    $boundary = "cutHere";
    header("Content-Type: multipart/x-mixed-replace; boundary=$boundary");
    echo("$boundary\r\n");
    echo("Content-Type: image/png\r\n\r\n");
    echo(file_get_contents("green-100x100.png"));
?>
