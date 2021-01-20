<?php
if (isset($_SERVER["HTTP_ACCESS_CONTROL_REQUEST_METHOD"]) &&
    isset($_COOKIE["foo"])) {
    header("HTTP/1.1 400 Bad Request");
} else {
    header("Cache-Control: no-store");
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    header("Access-Control-Allow-Credentials: true");
    header("Access-Control-Allow-Headers: X-Proprietary-Header");
    header("Connection: close");
    echo $_COOKIE["foo"];
}
?>
