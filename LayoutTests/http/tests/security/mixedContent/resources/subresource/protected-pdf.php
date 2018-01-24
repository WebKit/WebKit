<?php
header("Cache-Control: no-store");
header("Connection: close");
if (!isset($_SERVER["PHP_AUTH_USER"])) {
    header("WWW-authenticate: Basic realm=\"" . $_SERVER["REQUEST_URI"] . "\"");
    header("HTTP/1.0 401 Unauthorized");
    echo "<script>alert('Unauthorized'); window.testRunner && window.testRunner.notifyDone()</script>";
    exit;
}
// Authenticated
header("Content-Type: application/pdf");
header("Content-Disposition: attachment; filename=test.pdf");
echo file_get_contents("../../../../media/resources/test.pdf");
?>
