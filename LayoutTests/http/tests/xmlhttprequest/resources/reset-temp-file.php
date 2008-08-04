<?php
require_once 'portabilityLayer.php';

$tempDir = sys_get_temp_dir();
$tmpFile = $tempDir . $_GET['filename'];
unlink($tmpFile)
?>
