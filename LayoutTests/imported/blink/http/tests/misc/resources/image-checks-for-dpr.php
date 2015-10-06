<?php
    $dpr = $_SERVER["HTTP_DPR"];

    if(isset($dpr)) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
