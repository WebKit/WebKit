<?php
    $code = $_GET['code'];
    if (!isset($code))
        $code = 302;
    header('HTTP/1.1 ' . $code);

    $url = $_GET['url'];
    $random_id = $_GET['random_id'];
    if (isset($random_id)) {
        $id = '';
        $charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789';
        for ($i = 0; $i < 16; $i++)
            $id .= $charset[rand(0, strlen($charset) - 1)];
        header('Location: ' . $url . '?id=' . $id);
    }
    else
        header('Location: ' . $url);

    $expiration = $_GET['expiration'];
    if (isset($expiration)) {
        $expires = gmdate(DATE_RFC1123, time() + $expiration);
        header('Expires: ' . $expires);
    }

    $cache_control = $_GET['cache_control'];
    header('Cache-Control: ' . $cache_control);
?>
