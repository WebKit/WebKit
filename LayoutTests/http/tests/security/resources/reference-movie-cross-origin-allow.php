<?php

if (isset($_SERVER["HTTP_IF_MODIFIED_SINCE"]) || isset($_SERVER["HTTP_IF_NONE_MATCH"])) {
    // Always respond to conditional requests with a 304.
    header("HTTP/1.1 304 Not Modified");
    return;
}

header("Access-Control-Allow-Origin: *");
header("ETag: foo");
header("Last-Modified: Thu, 01 Jan 2000 00:00:00 GMT");
header("Cache-Control: max-age=0");

@include("../../media/resources/reference.mov");

?>
