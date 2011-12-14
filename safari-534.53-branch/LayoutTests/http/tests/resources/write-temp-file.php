<?php
require_once 'portabilityLayer.php';

if (!sys_get_temp_dir()) {
    echo "FAIL: No temp dir was returned.\n";
    exit();
}

$tmpFile = sys_get_temp_dir() . "/" . $_GET['filename'];

if (!file_put_contents($tmpFile, $_GET['data'])) {
    echo "FAIL: unable to write to file: " . $tmpFile . "\n";
    exit();
}

echo $tmpFile;
?>
