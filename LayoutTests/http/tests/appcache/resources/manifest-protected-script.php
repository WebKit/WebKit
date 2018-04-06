<?php

if (isset($_COOKIE['foo']) and $_COOKIE['foo'] == 'manifest') {
    header('Content-Type: text/html; ' . $_COOKIE['foo']);
    print("CACHE MANIFEST\n");
    print("simple.txt\n");
    return;
}
header('HTTP/1.0 404 Not Found');
header('Content-Type: text/html; ' . count($_COOKIE));

?>
