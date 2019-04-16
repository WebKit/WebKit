<?php
require_once '../../resources/portabilityLayer.php';

if (isset($_GET["nonce"]))
    $conversionFileName = "/adClickConversion" . $_GET["nonce"] . ".txt";
else
    $conversionFileName = "/adClickConversion.txt";
$conversionFilePath = sys_get_temp_dir() . $conversionFileName;
?>
