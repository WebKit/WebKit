<?php
require_once 'ping-file-path.php';

while (!file_exists($pingFilePath)) {
    usleep(10000);
    // file_exists() caches results, we want to invalidate the cache.
    clearstatcache();
}

echo "<html><body>\n";
echo "Ping sent successfully";
$pingFile = fopen($pingFilePath, 'r');
while ($line = fgets($pingFile)) {
    echo "<br>";
    echo trim($line);
}
fclose($pingFile);
unlink($pingFilePath);
echo "<script>";
echo "if (window.testRunner)";
echo "    testRunner.notifyDone();";
echo "</script>";
echo "</body></html>";
?>
