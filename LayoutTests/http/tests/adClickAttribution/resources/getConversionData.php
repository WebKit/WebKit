<?php
require_once 'conversionFilePath.php';

$noTimeout = True;
$timeoutMsecs = 0;
if (isset($_GET['timeout_ms'])) {
    $noTimeout = False;
    $timeoutMsecs = (int) $_GET['timeout_ms'];
}

$conversionFileFound = False;
while ($noTimeout || $timeoutMsecs > 0) {
    if (file_exists($conversionFilePath)) {
        $conversionFileFound = True;
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

if ($conversionFileFound) {
    echo "Conversion received.";
    $conversionFile = fopen($conversionFilePath, 'r');
    while ($line = fgets($conversionFile)) {
        echo "<br>";
        echo trim($line);
    }
    echo "<br>";
    fclose($conversionFile);
    unlink($conversionFilePath);
} else {
    echo "Conversion not received - timed out.<br>";
}

if (isset($_GET['endTest'])) {
    echo "<script>";
    echo "if (window.testRunner) {";
    echo "    testRunner.notifyDone();";
    echo "    testRunner.setAdClickAttributionOverrideTimerForTesting(false);";
    echo "    testRunner.setAdClickAttributionConversionURLForTesting('');";
    echo "}";
    echo "</script>";
}

echo "</body></html>";
?>
