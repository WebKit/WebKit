<?php
$file = '../../resources/balls-of-the-orient.aif';

if (file_exists($file)) {
    header("Status: 200 OK");
    header("HTTP/1.1 200 OK");
    header("Access-Control-Allow-Origin: *");
    header("Content-Type: audio/x-aiff");
    header('Content-Length: ' . filesize($file));
    ob_clean();
    flush();
    readfile($file);
    exit;
}
?>