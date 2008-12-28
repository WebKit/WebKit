<?php
require_once 'portabilityLayer.php';

// This script acts as a stateful proxy for retrieving files. When the state is set to
// offline, it simulates a network error by redirecting to itself.

if (!sys_get_temp_dir()) {
    echo "FAIL: No temp dir was returned.\n";
    exit();
}

function setState($newState, $file)
{
    file_put_contents($file, $newState);
}

function getState($file)
{
    if (!file_exists($file)) {
        return "Uninitialized";
    }
    return file_get_contents($file);
}

function contentType($path)
{
    if (preg_match("/\.html$/", $path))
        return "text/html";
    if (preg_match("/\.manifest$/", $path))
        return "text/cache-manifest";
    if (preg_match("/\.js$/", $path))
        return "text/javascript";
    return "text/plain";
}

$stateFile = sys_get_temp_dir() . "/network-simulator-state";
$command = $_GET['command'];
if ($command) {
    if ($command == "connect")
        setState("Online", $stateFile);
    else if ($command == "disconnect")
        setState("Offline", $stateFile);
    else
        echo "Unknown command: " . $command . "\n";
    exit();
}

$requestedPath = $_GET['path'];
$state = getState($stateFile);
if ($state == "Offline") {
    header('HTTP/1.1 307 Temporary Redirect');
    header('Location: ' . $_SERVER['REQUEST_URI']);
} else {
    // A little securuty checking can't hurt.
    if (strstr($requestedPath, ".."))
        exit;

    if ($requestedPath[0] == '/')
        $requestedPath = '..' . $requestedPath;

    header("Last-Modified: " . gmdate("D, d M Y H:i:s T", filemtime($requestedPath)));
    header("Content-Type: " . contentType($requestedPath));
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-cache, must-revalidate");
    header("Pragma: no-cache");

    print file_get_contents($requestedPath);
}
?>
