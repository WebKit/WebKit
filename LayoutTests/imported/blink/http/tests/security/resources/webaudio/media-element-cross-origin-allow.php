<?php

if (isset($_GET['with_credentials'])) {
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    header("Access-Control-Allow-Credentials: true");
} else {
    header("Access-Control-Allow-Origin: *");
}

$name = 'laughter.wav';
$fp = fopen($name, 'rb');

header("Content-Type: audio/x-wav");
header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
