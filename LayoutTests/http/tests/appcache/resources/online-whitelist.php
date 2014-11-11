<?php
require_once '../../resources/portabilityLayer.php';

# This script may only be used by appcache/online-whitelist.html test, since it uses global data.
$tmpFile = sys_get_temp_dir() . "/" . "appcache_online-whitelist_state";

function stepCounter($file)
{
    if (!file_exists($file)) {
        file_put_contents($file, "0");
        return "0";
    }
    $value = file_get_contents($file);
    file_put_contents($file, ++$value);
    return $value;
}

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: text/plain");

print(stepCounter($tmpFile));
?>
