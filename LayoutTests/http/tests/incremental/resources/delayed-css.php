<?php
header("Cache-Control: no-cache, no-store");
header("Content-Type: text/css");

$delay = $_GET['delay'];
usleep($delay * 1000);
?>
.delayed { background-color: green !important }
