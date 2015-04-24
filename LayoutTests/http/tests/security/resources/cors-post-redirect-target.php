<?php
header("Cache-Control: no-store");

$sawOrigin = false;
$originHeader = $_SERVER['HTTP_ORIGIN'];
if ($originHeader) {
    echo "Origin header value: $originHeader";
    $sawOrigin = true;
}

if (!$sawOrigin)
    echo "There was no origin header";

?>
<script>
if (window.testRunner)
    testRunner.notifyDone();
</script>
