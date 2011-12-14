<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, no-store, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: application/x-no-buffering-please");

$iteration = $_GET['iteration'];
$delay = $_GET['delay'];

echo "$iteration\n";
echo "$delay\n";

for ($i = 1; $i <= $iteration; ++$i) {
    echo "$i\n";
    echo "foobar\n";
    // Force content to be sent to the browser as is.
    ob_flush();
    flush();
    usleep($delay * 1000);
}
?>
