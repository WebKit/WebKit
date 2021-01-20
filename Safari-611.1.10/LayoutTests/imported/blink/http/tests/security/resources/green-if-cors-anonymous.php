<?php
if (isset($_SERVER['HTTP_ORIGIN']) && !isset($_COOKIE['cookie'])) {
    header("Access-Control-Allow-Origin: *");
    $name = 'green-background.css';
} else {
    $name = 'red-background.css';
}

$fp = fopen($name, 'rb');
header("Content-Type: text/css");
header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
