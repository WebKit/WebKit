<?php
$text = file_get_contents("./lorem.txt");
$gzipText = gzencode($text, 6);

header("Content-Type: text/plain");
header("Content-Encoding: gzip");
header("Content-Length: " . strlen($gzipText));

echo $gzipText;
?>
