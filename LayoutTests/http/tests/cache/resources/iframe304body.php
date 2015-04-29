<?php
require_once '../../resources/portabilityLayer.php';

clearstatcache();

if ($_SERVER["HTTP_IF_MODIFIED_SINCE"]) {
    header("HTTP/1.0 304 Not Modified");
    exit();
}
header('Cache-Control: no-cache');
header('Content-Type: text/html');
header('Etag: 123456789');
?>
body text