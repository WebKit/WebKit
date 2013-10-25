<?php
$allowOrigin = $_GET['allow'];
if ($allowOrigin == "true") {
    header("Access-Control-Allow-Origin: *");
}

$file = $_GET['file'];
$fp = fopen($file, 'rb');
header("Content-Type: image/png");
header("Content-Length: " . filesize($file));

fpassthru($fp);
exit;
?>
