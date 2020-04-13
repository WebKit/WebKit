<?php
header("Cache-Control: no-store");
header("Connection: close");
header("Content-Type: application/javascript");
echo "import \"" . $_GET["url"] . "\"";
?>
