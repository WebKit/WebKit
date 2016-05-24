<?php
    $url = $_GET["url"];

    $enableCaching = isset($_SERVER["ENABLECACHING"]) ? true : false;
    $code = isset($_GET["code"]) ? $_GET["code"] : 302;

    header("HTTP/1.1 $code");
    header("Location: $url");

    if ($enableCaching)
        header("Cache-Control: max-age=31536000");
    else
        header("Cache-Control: no-store");
?>
