<?php
$delay = $_GET['delay'];
usleep($delay * 1000);

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, no-store, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: application/x-no-buffering-please");

echo "$delay\n";
echo "foobar\n";
ob_flush();
flush();
?>
