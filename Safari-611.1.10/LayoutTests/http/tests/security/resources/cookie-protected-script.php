<?php
header("Cache-Control: no-store");
header("Connection: close");
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Credentials: true");
if (!isset($_COOKIE["ModuleSecret"])) {
    header("HTTP/1.0 404 Not Found");
    exit;
}
// Authenticated
header("Content-Type: application/javascript");
echo "";
?>
