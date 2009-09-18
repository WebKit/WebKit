<?php
header("Set-Cookie: WK-cross-origin=1");
header("Cache-Control: no-store");
header("Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT");
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Connection: close");

if (!isset($_SERVER['PHP_AUTH_USER'])) {
    header('WWW-Authenticate: Basic realm="WebKit xmlhttprequest/cross-origin-no-authorization"');
    header('HTTP/1.0 401 Unauthorized');
    echo 'Authentication canceled';
    exit;
}
?>
log('PASS: Loaded, user <?php print $_SERVER['PHP_AUTH_USER'] ?>');
