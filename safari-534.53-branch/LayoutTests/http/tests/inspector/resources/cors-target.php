<?php
    parse_str($_SERVER['QUERY_STRING'], $queryParams);
    if ($queryParams["deny"] != "yes") {
        header("Access-control-allow-origin: http://127.0.0.1:8000");
        header("Access-control-allow-headers: Content-Type");
    }
    header("Content-Type: text/plain");

    print "body"
?>
