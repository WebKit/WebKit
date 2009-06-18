<?php
header("Cache-Control: no-store");
header("Last-Modified: Thu, 19 Mar 2009 11:22:11 GMT");
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Credentials: true");

if (isset($_COOKIE['WK-cross-origin'])) {
    echo 'WK-cross-origin: ' . $_COOKIE['WK-cross-origin'];
} else {
    echo 'Cross-origin cookie was not sent.';
}
?>
