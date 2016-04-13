<?php

function send304()
{
    header("HTTP/1.1 304 Not Modified");
    header("ETag: foo");
}

$id = $_GET["id"];
$count = 1;
if (isset($_COOKIE[$id])) {
    $count = $_COOKIE[$id];
}

setcookie($id, $count + 1);

if ($count == 1) {
    header("Cache-Control: must-revalidate");
    header("ETag: foo");
    header("Content-Type: application/json");
    echo '{"version": 1}';
} else if ($count == 2) {
    send304();
} else if ($count == 3) {
    header("HTTP/1.1 404 Not Found");
    header("Cache-Control: must-revalidate");
    header("Content-Type: application/json");
    echo '{"not": "found"}';
} else if ($count == 4) {
    if ($_SERVER["HTTP_IF_NONE_MATCH"] == "foo") {
        send304();
    } else {
        header("Cache-Control: must-revalidate");
        header("ETag: foo");
        header("Content-Type: application/json");
        echo '{"version": 2}';
    }
} else {
    send304();
}

?>
