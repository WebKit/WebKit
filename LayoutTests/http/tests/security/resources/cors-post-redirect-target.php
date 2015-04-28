<?php
header("Cache-Control: no-store");

$sawOrigin = false;
$originHeader = isset($_SERVER['HTTP_ORIGIN']) ? $_SERVER['HTTP_ORIGIN'] : null;
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
