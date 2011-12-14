<?php
if(!isset($_SERVER['HTTP_ORIGIN'])) {
    echo "FAIL: No origin header sent";
} else {
    header("Access-Control-Allow-Origin: *");
    header("Access-Control-Allow-Headers: X-Proprietary-Header");
    echo "PASS: Origin header correctly sent";
}
?>
