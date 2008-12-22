<?php
require_once '../../xmlhttprequest/resources/portabilityLayer.php';

$tmpFile = ensureTrailingSlash(sys_get_temp_dir()) . "abort_update_state";

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

$state = getState($tmpFile);

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-store, no-cache, must-revalidate");
header("Pragma: no-cache");

if ($state == "Uninitialized") {
    setState("ManifestSent", $tmpFile);
    header("Content-Type: text/cache-manifest");
    print("CACHE MANIFEST\n");
} else if ($state == "ManifestSent") {
    unlink($tmpFile);
    header('HTTP/1.0 404 Not Found');
}
?>
