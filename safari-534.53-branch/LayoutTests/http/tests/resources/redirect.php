<?php
    $url = $_GET['url'];
    $refresh = $_GET['refresh'];
    
    if (isset($refresh)) {
        header("HTTP/1.1 200");
        header("Refresh: $refresh; url=$url");
        return;
    }

    $code = $_GET['code'];
    if (!isset($code))
        $code = 302;
    header("HTTP/1.1 $code");
    header("Location: $url");
?>
