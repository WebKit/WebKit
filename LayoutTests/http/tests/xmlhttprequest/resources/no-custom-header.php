<?php
require_once '../../resources/portabilityLayer.php';

$stateFile = sys_get_temp_dir() . "/access-control-preflight-headers-status";

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

header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Headers: X-Custom-Header");
header("Access-Control-Max-Age: 0");

if ($_SERVER["REQUEST_METHOD"] == "OPTIONS") {
    if (isset($_SERVER["HTTP_X_CUSTOM_HEADER"]))
        setState("FAIL", $stateFile);
    else
        setState("PASS", $stateFile);
} else {
    if (isset($_SERVER["HTTP_X_CUSTOM_HEADER"]))
        echo getState($stateFile);
    else
        echo "FAIL - no header in actual request";
}
?>
