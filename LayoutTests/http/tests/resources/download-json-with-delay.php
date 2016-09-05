<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");

if ($_GET['allowCache']) {
    header("Content-Type: application/json");
} else {
    header("Content-Type: application/x-no-buffering-please");
    header("Cache-Control: no-cache, no-store, must-revalidate");
    header("Pragma: no-cache");
}

if ($_GET['cors']) {
    header("Access-Control-Allow-Origin: *");
}

$iteration = $_GET['iteration'];
$delay = $_GET['delay'];

echo "[$iteration, $delay ";

for ($i = 1; $i <= $iteration; ++$i) {
    echo ", $i, \"foobar\"";
    // Force content to be sent to the browser as is.
    ob_flush();
    flush();
    usleep($delay * 1000);
}
echo "]";
?>
