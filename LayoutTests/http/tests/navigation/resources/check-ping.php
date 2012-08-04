<?php
while (!file_exists("ping.txt")) {
    usleep(10000);
    // file_exists() caches results, we want to invalidate the cache.
    clearstatcache();
}

echo "<html><body>\n";
echo "Ping sent successfully";
$pingFile = fopen("ping.txt", 'r');
while ($line = fgets($pingFile)) {
    echo "<br>";
    echo trim($line);
}
fclose($pingFile);
unlink("ping.txt");
echo "<script>";
echo "if (window.testRunner)";
echo "    testRunner.notifyDone();";
echo "</script>";
echo "</body></html>";
?>
