<?php
    $url = $_GET["url"];

    $code = isset($_GET["code"]) ? $_GET["code"] : 302;

    header("HTTP/1.1 $code");
    header("Location: $url");
    header("Access-Control-Allow-Origin: *");

    # Workaround for https://bugs.webkit.org/show_bug.cgi?id=77538
    # Caching redirects results in flakiness in tests that dump loader delegates.
    header("Cache-Control: no-store");
?>
