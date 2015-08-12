<?php
if ($_SERVER["HTTP_IF_MODIFIED_SINCE"]) {
    header("HTTP/1.0 304 Not Modified");
    exit();
}
header("Cache-control: no-cache");
header("Last-Modified: Thu, 01 Jan 1970 00:00:00 GMT");
header("Content-Type: text/plain");
?>
foo
