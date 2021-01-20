<?php
require_once '../../../resources/portabilityLayer.php';

$beaconFilename = sys_get_temp_dir() . "/beacon" . (isset($_REQUEST['name']) ? $_REQUEST['name'] : "") . ".txt";

$max_attempts = 700;
$retries = isset($_REQUEST['retries']) ? (int)$_REQUEST['retries'] : $max_attempts;
while (!file_exists($beaconFilename) && $retries != 0) {
    usleep(10000);
    # file_exists() caches results, we want to invalidate the cache.
    clearstatcache();
    $retries--;
}

header('Content-Type: text/plain');
header('Access-Control-Allow-Origin: *');
if (file_exists($beaconFilename)) {
    $beaconFile = false;
    if (is_readable($beaconFilename)) {
        $beaconFile = fopen($beaconFilename, 'r');
    }
    if ($beaconFile) {
        echo "Beacon sent successfully\n";
        while ($line = fgets($beaconFile)) {
            $trimmed = trim($line);
            if ($trimmed != "")
                echo "$trimmed\n";
        }
        fclose($beaconFile);
        unlink($beaconFilename);
    } else {
        echo "Beacon status not readable\n";
    }
} else {
    echo "Beacon not sent\n";
}
?>
