<?php
require_once 'tokenSigningFilePath.php';

$noTimeout = True;
$timeoutMsecs = 0;
if (isset($_GET['timeout_ms'])) {
    $noTimeout = False;
    $timeoutMsecs = (int) $_GET['timeout_ms'];
}

$tokenSigningFileFound = False;
while ($noTimeout || $timeoutMsecs > 0) {
    if (file_exists($tokenSigningFilePath)) {
        $tokenSigningFileFound = True;
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

if ($tokenSigningFileFound) {
    echo "Token signing request received.";
    $tokenSigningFile = fopen($tokenSigningFilePath, 'r');
    while ($line = fgets($tokenSigningFile)) {
        echo "<br>";
        echo trim($line);
    }
    echo "<br>";
    fclose($tokenSigningFile);
    unlink($tokenSigningFilePath);
} else {
    echo "Token signing request not received - timed out.<br>";
}

if (isset($_GET['endTest'])) {
    echo "<script>";
    echo "if (window.testRunner) {";
    echo "    testRunner.setPrivateClickMeasurementOverrideTimerForTesting(false);";
    echo "    testRunner.setPrivateClickMeasurementTokenPublicKeyURLForTesting('');";
    echo "    testRunner.setPrivateClickMeasurementTokenSignatureURLForTesting('');";
    echo "    testRunner.notifyDone();";
    echo "}";
    echo "</script>";
}

echo "</body></html>";
?>
