<?php
    $boundary = "cutHere";

    function sendPart($data)
    {
        global $boundary;

        echo("Content-Type: image/png\r\n\r\n");
        echo($data);
        echo("--$boundary\r\n");
        flush();
    }

    $blue = file_get_contents("1x1-blue.png");

    header("Content-Type: multipart/x-mixed-replace; boundary=$boundary");

    echo("--$boundary\r\n");
    sendPart($blue);
    sendPart($blue);
    sleep(10);
?>
