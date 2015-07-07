<?php
require_once "../../resources/portabilityLayer.php";

function getRequestCount($file)
{
    if (!file_exists($file)) {
        return 0;
    }
    return (int)file_get_contents($file);
}

function setRequestCount($file, $count)
{
    file_put_contents($file, $count);
}

$tmpFile = sys_get_temp_dir() . "/" . $_GET["filename"];

$currentCount = getRequestCount($tmpFile);
$mode = $_GET["mode"];

if ($mode == "getFont") {
    setRequestCount($tmpFile, $currentCount + 1);
    header("Access-control-max-age: 0");
    header("Access-control-allow-origin: *");
    header("Access-control-allow-methods: *");
    header("Cache-Control: max-age=0");
    header("Content-Type: application/octet-stream");
    echo "";
} else if ($mode == "getRequestCount") {
    header("Access-control-max-age: 0");
    header("Access-control-allow-origin: *");
    header("Access-control-allow-methods: *");
    echo $currentCount;
}
?>
