<?php
header("Cache-Control: no-store");
header("Connection: close");
if (!isset($_SERVER["PHP_AUTH_USER"])) {
    header("WWW-authenticate: Basic realm=\"" . $_SERVER["REQUEST_URI"] . "\"");
    header("HTTP/1.0 401 Unauthorized");
    exit;
}
// Authenticated
header("Content-Type: text/css");
echo file_get_contents("../../../contentSecurityPolicy/resources/style-set-red.css");
?>
