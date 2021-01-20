<?php
$status_code = $_GET['status-code'];
header("HTTP/1.1 $status_code OK");

switch ($status_code) {
case 200:
    header("Content-Type: text/event-stream");
    echo "data: hello\n\n";
    break;
case 301:
case 302:
case 303:
case 307:
    $uri = rtrim(dirname($_SERVER['PHP_SELF']), '/\\') . "/simple-event-stream.asis";
    header("Location: http://" . $_SERVER['HTTP_HOST'] . $uri);
    break;
}
?>
