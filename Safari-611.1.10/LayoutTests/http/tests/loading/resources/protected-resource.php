<?php
header("Cache-Control: no-store");
header("Connection: close");
if (!isset($_SERVER['PHP_AUTH_USER'])) {
    header("WWW-authenticate: Basic realm=\"" . $_SERVER['REQUEST_URI'] . "\"");
    header('HTTP/1.0 401 Unauthorized');
    exit;
}
?>
Authenticated as user: <?php print (string)$_SERVER['PHP_AUTH_USER']?> password: <?php print (string)$_SERVER['PHP_AUTH_PW']?>
