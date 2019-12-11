<?php
header("Cache-Control: no-store");
header("Connection: close");
if (!isset($_SERVER["PHP_AUTH_USER"])) {
    header("WWW-authenticate: Basic realm=\"" . $_SERVER["REQUEST_URI"] . "\"");
    header("HTTP/1.0 401 Unauthorized");
    exit;
}
// Authenticated
header("Content-Type: image/png");
echo file_get_contents("../../contentSecurityPolicy/block-all-mixed-content/resources/red-square.png");
?>
