<?php
require_once "report-file-path.php";

while (!file_exists($reportFilePath)) {
    usleep(10000);
    // file_exists() caches results, we want to invalidate the cache.
    clearstatcache();
}

echo "<html><body>\n";
echo "CSP report received:";
$reportFile = fopen($reportFilePath, 'r');
while ($line = fgets($reportFile)) {
    echo "<br>";
    echo trim($line);
}
fclose($reportFile);
unlink($reportFilePath);
echo "<script>";
echo "if (window.testRunner)";
echo "    testRunner.notifyDone();";
echo "</script>";
echo "</body></html>";
?>
