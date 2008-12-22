<?php
require_once 'portabilityLayer.php';

$tmpFile = ensureTrailingSlash(sys_get_temp_dir()) . $_GET['filename'];
unlink($tmpFile)
?>
