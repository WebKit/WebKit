<?php
header("Cache-Control: no-store");
header("Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT");
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Credentials: true");
header("Connection: close");

$cookie = $_GET['cookie'];
if (empty($cookie)) {
    $cookie = 'WK-cross-origin';
}

if (isset($_COOKIE[$cookie])) {
    echo $cookie . ': ' . $_COOKIE[$cookie];
} else {
    echo 'Cookie was not sent.';
}
?>
