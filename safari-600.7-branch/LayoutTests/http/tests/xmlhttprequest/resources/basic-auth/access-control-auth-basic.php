<?php

header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Credentials: true");

if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
    if (!isset($_COOKIE['foo'])) {
        header("Access-Control-Allow-Methods: PUT");
	}
} else {
    if (!isset($_SERVER['PHP_AUTH_USER']) || !isset($_REQUEST['uid']) || ($_REQUEST['uid'] != $_SERVER['PHP_AUTH_USER'])) {
        header('WWW-Authenticate: Basic realm="WebKit Test Realm/Cross Origin"');
        header('HTTP/1.0 401 Unauthorized');
        echo 'Authentication canceled';
        exit;
    } else {
        echo "User: {$_SERVER['PHP_AUTH_USER']}, password: {$_SERVER['PHP_AUTH_PW']}.";
    }
}
?>
