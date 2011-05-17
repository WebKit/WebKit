<?php
$status_code = $_GET['status'];

$uri = rtrim(dirname($_SERVER['PHP_SELF']), '/\\') . "/" . $_GET['target'];

switch ($status_code) {
    case 301:
        header("HTTP/1.1 301 Moved Permanently");
        header("Location: http://" . $_SERVER['HTTP_HOST'] . $uri);
        break;
    case 302:
        header("HTTP/1.1 302 Found");
        header("Location: http://" . $_SERVER['HTTP_HOST'] . $uri);
        break;
    case 303:
        header("HTTP/1.1 303 See Other");
        header("Location: http://" . $_SERVER['HTTP_HOST'] . $uri);
        break;
    case 307:
        header("HTTP/1.1 307 Temporary Redirect");
        header("Location: http://" . $_SERVER['HTTP_HOST'] . $uri);
        break;
    default:
        header("HTTP/1.1 500 Internal Server Error");
        echo "Unexpected status code ($status_code) received.";
}
?>
