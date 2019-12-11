<?php
require_once '../../../resources/portabilityLayer.php';

if (isset($_GET['test'])) {
    $reportFilePath = sys_get_temp_dir() . "/" . str_replace("/", "-", $_GET['test']) . ".csp-report.txt"; 
} elseif (isset($_SERVER["HTTP_REFERER"]) and strpos($_SERVER["HTTP_REFERER"], '/resources/') === false) {
    $reportFilePath = sys_get_temp_dir() . "/" . str_replace("/", "-", parse_url($_SERVER["HTTP_REFERER"], PHP_URL_PATH)) . ".csp-report.txt"; 
} else {
    header("HTTP/1.1 500 Internal Server Error");
    echo "This script needs to know the name of the test to form a unique temporary file path. It can get one either from HTTP referrer, or from a 'test' parameter.\n";
    exit();
}

?>
