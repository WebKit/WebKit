<?php
require_once 'ping-file-path.php';

$noTimeout = True;
$timeoutMsecs = 0;
if (isset($_GET['timeout_ms'])) {
    $noTimeout = False;
    $timeoutMsecs = (int) $_GET['timeout_ms'];
}

$pingFileFound = False;
while ($noTimeout || $timeoutMsecs > 0) {
    if (file_exists($pingFilePath)) {
        $pingFileFound = True;
        break;
    }
    $sleepMsecs = 10;
    usleep($sleepMsecs * 1000);
    if (!$noTimeout) {
        $timeoutMsecs -= $sleepMsecs;
    }
    // file_exists() caches results, we want to invalidate the cache.
    clearstatcache();
}


echo "<html><body>\n";

if ($pingFileFound) {
    echo "Ping received.";
    $pingFile = fopen($pingFilePath, 'r');
    while ($line = fgets($pingFile)) {
        echo "<br>";
        echo trim($line);
    }
    fclose($pingFile);
    unlink($pingFilePath);
} else {
    echo "Ping not received - timed out.";
}

if (isset($_GET['end_test'])) {
    echo "<script>";
    echo "if (window.testRunner)";
    echo "    testRunner.notifyDone();";
    echo "</script>";
}

echo "</body></html>";
?>
