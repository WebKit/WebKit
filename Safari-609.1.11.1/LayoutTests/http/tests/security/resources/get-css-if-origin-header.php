<?php
header("Cache-Control: no-store");
header("Content-Type: text/css");

$id = isset($_GET['id']) ? $_GET['id'] : "id";
$originHeader = isset($_SERVER['HTTP_ORIGIN']) ? $_SERVER['HTTP_ORIGIN'] : null;
if ($originHeader) {
    header("Access-Control-Allow-Origin: $originHeader");
    echo "." . $id . " { background-color: yellow; }";
} else {
    echo "." . $id . " { background-color: blue; }";
}

?>
