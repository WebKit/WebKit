<?php
if ($_SERVER["HTTP_IF_NONE_MATCH"] == "foo") {
    header("HTTP/1.1 304 Not Modified");
    header("ETag: foo");
    return;
}

header("Content-Type: text/css");
header("ETag: foo");
header("Cache-Control: max-age=0");

?>

.testClass
{
    color: green;
}
