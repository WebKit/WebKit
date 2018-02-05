<?php
$name = $_GET['name'];
$stallAt = $_GET['stallAt'];
$stallFor = $_GET['stallFor'];
$stallRepeat = $_GET['stallRepeat'];
$mimeType = $_GET['mimeType'];

$file = fopen($name, "rb");
if (!$file)
    die("Cannot open file.");

header("Content-Type: " . $mimeType);
header("Content-Length: " . filesize($name));

if (isset($stallAt) && isset($stallFor)) {
    $stallAt = (int)$stallAt;
    if ($stallAt > filesize($name))
        die("Incorrect value for stallAt.");

    $writtenTotal = 0;
    $fileSize = filesize($name);

    while ($writtenTotal < $fileSize) {
        $stallAt = min($stallAt, $fileSize - $writtenTotal);
        $written = 0;

        while ($written < $stallAt) {
            $write = min(1024, $stallAt - $written);
            echo(fread($file, $write));
            $written += $write;
            flush();
            ob_flush();
        }

        $writtenTotal += $written;
        $remaining = $fileSize - $writtenTotal;
        if (!$remaining)
            break;

        usleep($stallFor * 1000000);
        if (!isset($stallRepeat) || !$stallRepeat)
            $stallAt = $remaining;
    }
} else {
    echo(fread($file, filesize($name)));
    flush();
    ob_flush();
}
fclose($file);
?>
