<?php
    $url = $_GET['url'];
    $code = $_GET['code'];
    if (!isset($code))
        $code = 302;
    header("HTTP/1.1 $code");
    header("Location: $url");
?>
