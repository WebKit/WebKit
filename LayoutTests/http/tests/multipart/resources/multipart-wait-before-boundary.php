<?php
    if (isset($_GET['done'])) {
      header("Content-Type: text/html");
      echo("<script>parent.success()</script>");
      exit(0);
    }

    $boundary = "cutHere";

    function sendHeader()
    {
        global $boundary;

        echo("--$boundary\r\n");
        echo("Content-Type: text/html\r\n\r\n");
        flush();
    }

    header("Content-Type: multipart/x-mixed-replace; boundary=$boundary");

    // generate some padding to work around CFNetwork handling of multipart data
    $padding = "aa";
    for ($i = 0; $i < 10; $i++) {
      $padding .= $padding;
    }

    sendHeader();
    ob_end_flush();
    echo("test html\n");
    echo("<!-- $padding -->");
    flush();
    sendHeader();
    echo("second html");
    echo("<script>parent.childLoaded()</script>");
    echo("<!-- $padding -->");
    flush();
    sendHeader();
    echo("third html");
    echo("<!-- $padding -->");
    flush();
    usleep(30 * 1000000);
?>
