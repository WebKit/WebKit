<?php
$query = $_GET["q"];
$contentType = !empty($_GET["type"]) ? $_GET["type"] : "text/plain";
$gzipEncodedData = gzencode($_GET['q']);

header("Content-Type: " . $_GET["type"]);
header("Content-Encoding: gzip");
header("Content-Length: " . strlen($gzipEncodedData));
echo $gzipEncodedData;
?>
