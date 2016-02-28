<?php
$sawOrigin = false;
foreach (getallheaders() as $name => $value) {
    if (strtolower($name) == "origin") {
        echo "Origin header value: $value";
        $sawOrigin = true;
    }
}

if (!$sawOrigin)
    echo "There was no origin header";

?>
<script>
if (window.testRunner)
    testRunner.notifyDone();
</script>
