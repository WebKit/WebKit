<?php
if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_REQUEST['uid']) || $_REQUEST['uid'] != $_SERVER['PHP_AUTH_USER']) {
    header('WWW-Authenticate: Basic realm="blink test realm"');
    header('HTTP/1.0 401 Unauthorized');
    echo 'Authentication cancelled.';
} else {
    header("Access-Control-Allow-Origin: *");
    $name = 'abe.png';
    $fp = fopen($name, 'rb');
    header("Content-Type: image/png");
    header("Content-Length: " . filesize($name));
    header('HTTP/1.0 200 OK');
    fpassthru($fp);
}
?>
