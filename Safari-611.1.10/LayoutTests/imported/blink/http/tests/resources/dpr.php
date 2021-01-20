<?php
$name = $_GET['name'];
$mimeType = $_GET['mimeType'];
$dpr = $_GET['dpr'];

header('Content-Type: ' . $mimeType);
header('Content-Length: ' . filesize($name));
if (isset($_GET['expires']))
  header('Cache-control: max-age=0'); 
else
  header('Cache-control: max-age=86400'); 
header('ETag: dprimage'); 

if ($_SERVER['HTTP_IF_NONE_MATCH'] == 'dprimage') {
  header('HTTP/1.1 304 Not Modified');
  exit;
}

header('Content-DPR: '. $dpr);

readfile($name);
