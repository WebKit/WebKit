<?php
$status = isset($_GET['status']) ? $_GET['status'] : 200;

if ($status == 204) {
    header("HTTP/1.1 204 No Content");
    exit;
}

if ($status == 404) {
    header("HTTP/1.1 404 Not Found");
    exit;
}

echo "Success";
?>
