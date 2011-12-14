<?php
require_once 'portabilityLayer.php';

// This script detects requests that could not be sent before cross-site XMLHttpRequest appeared.

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, no-store, must-revalidate");
header("Pragma: no-cache");

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
        return "";
    }
    return file_get_contents($file);
}

$stateFile = sys_get_temp_dir() . "/tripmine-status";
$command = $_GET['command'];
if ($command) {
    if ($command == "status")
        echo getState($stateFile);
    exit();
}

$method = $_SERVER['REQUEST_METHOD'];
$contentType = $_SERVER['CONTENT_TYPE'];

if ($method == "OPTIONS") {
    // Don't allow cross-site requests with preflight.
    exit();
}

// Only allow simple cross-site requests - since we did not allow preflight, this is all we should ever get.

if ($method != "GET" && $method != "HEAD" && $method != "POST") {
    setState("FAIL. Non-simple method $method.", $stateFile);
    exit();
}

if (isset($contentType)
     && !preg_match("/^application\/x\-www\-form\-urlencoded(;.+)?$/", $contentType)
     && !preg_match("/^multipart\/form\-data(;.+)?$/", $contentType)
     && !preg_match("/^text\/plain(;.+)?$/", $contentType)) {
    setState("FAIL. Non-simple content type: $contentType.", $stateFile);
    exit();
}

if (isset($_SERVER['HTTP_X_WEBKIT_TEST'])) {
    setState("FAIL. Custom header sent with a simple request.", $stateFile);
    exit();
}
?>
