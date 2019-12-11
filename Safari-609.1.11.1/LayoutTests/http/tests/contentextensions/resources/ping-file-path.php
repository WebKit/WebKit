<?php
require_once '../../resources/portabilityLayer.php';

if (isset($_GET['test'])) {
    $pingFilePath = sys_get_temp_dir() . "/" . str_replace("/", "-", $_GET['test']) . ".ping.txt"; 
}

?>
