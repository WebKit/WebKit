<?php
    $url = $_GET['url'];
    $code = $_GET['code'];
    header("HTTP/1.1 $code");
    header("Location: $url");
?>
