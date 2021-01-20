<?php
$text = file_get_contents("./lorem.txt");
$gzipText = gzencode($text, 6);

header("Content-Type: text/plain");
header("Content-Encoding: gzip");
flush(); // Send headers without Content-Length.

echo $gzipText;
?>
