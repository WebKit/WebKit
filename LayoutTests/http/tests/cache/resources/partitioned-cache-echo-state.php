<?php
require_once '../../resources/portabilityLayer.php';

header('Content-Type: application/javascript');
header('Cache-Control: max-age=3600');

# This script may only be used by cache/partitioned-cache.html test, since it uses global data.
$tmpFile = sys_get_temp_dir() . "/" . "cache-partitioned-cache-state";
?>
var response = '<?php print file_get_contents($tmpFile); ?>';
