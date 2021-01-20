<?php
$status = $_SERVER['HTTP_X_STATUS_CODE_NEEDED'];

if ($status == '304') {
    header("HTTP/1.1 304 Not Modified");
    exit;
}

header('Content-Type: text/plain');
header("Cache-Control: public, max-age=31556926"); // One year.


echo "Plain text resource.";
?>
