<?php
    header("Access-control-allow-headers: X-Requested-With");
    header("Access-control-max-age: 0");
    header("Access-control-allow-origin: *");
    header("Access-control-allow-methods: *");
    header("Vary: Accept-Encoding");
    header("Content-Type: text/plain");

    print "echo"
?>
