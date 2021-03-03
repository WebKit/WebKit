<?php
require_once '../../resources/portabilityLayer.php';

if (isset($_GET["dummy"]))
    $tokenSigningFileName = "/privateClickMeasurementTokenSigningRequest" . $_GET["dummy"] . ".txt";
else
    $tokenSigningFileName = "/privateClickMeasurementTokenSigningRequest.txt";
$tokenSigningFilePath = sys_get_temp_dir() . $tokenSigningFileName;
?>
