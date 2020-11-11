<?php
require_once '../../resources/portabilityLayer.php';

if (isset($_GET["nonce"]))
    $conversionFileName = "/privateClickMeasurementConversion" . $_GET["nonce"] . ".txt";
else
    $conversionFileName = "/privateClickMeasurementConversion.txt";
$conversionFilePath = sys_get_temp_dir() . $conversionFileName;
?>
