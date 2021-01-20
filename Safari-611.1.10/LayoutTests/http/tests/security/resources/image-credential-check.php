<?php
if ($_SERVER['PHP_AUTH_USER'] == 'testusername' && $_SERVER['PHP_AUTH_PW'] == 'testpassword') {
    $fp = fopen('black-square.png', 'rb');
    header("Content-Type: image/png");
    header("Content-Length: " . filesize('black-square.png'));
    fpassthru($fp);
    exit;
}
header('HTTP/1.0 401 Unauthorized');
header('WWW-Authenticate: Basic realm="test realm"');
exit;
?>
