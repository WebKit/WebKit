<?php
ob_end_clean();
header("Connection: close");
header("Content-Type: image/svg+xml");
header("Content-Length: 1234");
flush();
echo("<svg><sdfdfs");
flush();
?>

