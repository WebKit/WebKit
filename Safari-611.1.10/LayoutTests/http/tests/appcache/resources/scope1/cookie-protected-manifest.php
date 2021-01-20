<?php

if (isset($_COOKIE['scope']) and $_COOKIE['scope'] == 'manifest') {
    header('Content-Type: text/html; ' . $_COOKIE['scope']);
    print("CACHE MANIFEST\n");
    print("/appcache/resources/simple.txt\n");
    print("/appcache/resources/scope2/cookie-protected-script.php\n");
    print("/appcache/resources/cookie-protected-script.php\n");
    return;
}
header('HTTP/1.0 404 Not Found');
header('Content-Type: text/html; ' . $_COOKIE['scope']);

?>
