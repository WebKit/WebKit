<?php
$name = $_GET['name'];
$stallAt = $_GET['stallAt'];
$stallFor = $_GET['stallFor'];
$mimeType = $_GET['mimeType'];

$file = fopen($name, "rb");
if (!$file)
    die("Cannot open file.");

header("Content-Type: " . $mimeType);
header("Content-Length: " . filesize($name));

if (isset($stallAt) && isset($stallFor)) {
    $stallAt = (int)$stallAt;
    $stallFor = (int)$stallFor;
    if ($stallAt > filesize($name))
        die("Incorrect value for stallAt.");

    while ($written < $stallAt) {
        $write = 1024;
        if ($write > $stallAt - $written)
            $write = $stallAt - $written;

        echo(fread($file, $write));
        $written += $write;
        flush();
        ob_flush();
    }
    sleep($stallFor);
    echo(fread($file, filesize($name) - $stallAt));
} else {
    echo(fread($file, filesize($name)));
}

fclose($file);
?>
