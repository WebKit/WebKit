<?php
$filename = '../../../resources/square.png';
$filesize = filesize($filename);
$handle = fopen($filename, 'rb');
$contents = fread($handle, $filesize);
fclose($handle);

header("Content-Type: image/png");
header("Content-Security-Policy: default-src 'none'");
header('Content-Length: ' . $filesize);
echo $contents;

?>
