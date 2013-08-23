<?php
require_once '../../resources/portabilityLayer.php';

header('Content-Type: application/javascript');
header('Cache-Control: max-age=3600');

$tmpFile = sys_get_temp_dir() . "/" . "partitioned-cache-state";
?>
var response = '<?php print file_get_contents($tmpFile); ?>';
