<?php
$expectedUsername = isset($_GET['username']) ? $_GET['username'] : 'username';
$expectedPassword = isset($_GET['password']) ? $_GET['password'] : 'password';
$realm = isset($_GET['realm']) ? $_GET['realm'] : $_SERVER['REQUEST_URI'];

header("Cache-Control: no-store");
if (!isset($_SERVER['PHP_AUTH_USER']) || $_SERVER['PHP_AUTH_USER'] != $expectedUsername ||  
    !isset($_SERVER['PHP_AUTH_PW']) || $_SERVER['PHP_AUTH_PW'] != $expectedPassword) {
    header("WWW-Authenticate: Basic realm=\"" . $realm . "\"");
    header('HTTP/1.0 401 Unauthorized');
    print 'Sent username:password of (' . $_SERVER['PHP_AUTH_USER'] . ':' . $_SERVER['PHP_AUTH_PW'] . ') which is not what was expected';
    exit;
}
?>
Authenticated as user: <?php print (string)$_SERVER['PHP_AUTH_USER']?> password: <?php print (string)$_SERVER['PHP_AUTH_PW']?>
