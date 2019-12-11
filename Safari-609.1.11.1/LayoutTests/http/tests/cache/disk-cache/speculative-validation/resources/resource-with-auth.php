<?php
$expectedUsername = 'testUsername';
$expectedPassword = 'testPassword';
$realm = $_SERVER['REQUEST_URI'];

header('Content-Type: text/javascript');
header('Cache-Control: max-age=0');
header('Etag: 123456789');

if (!isset($_SERVER['PHP_AUTH_USER']) || $_SERVER['PHP_AUTH_USER'] != $expectedUsername ||
    !isset($_SERVER['PHP_AUTH_PW']) || $_SERVER['PHP_AUTH_PW'] != $expectedPassword) {
    header("WWW-Authenticate: Basic realm=\"" . $realm . "\"");
    header('HTTP/1.0 401 Unauthorized');
    print 'Sent username:password of (' . $_SERVER['PHP_AUTH_USER'] . ':' . $_SERVER['PHP_AUTH_PW'] . ') which is not what was expected';
    exit;
}
?>
loaded = true;
