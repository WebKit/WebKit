<?php
require_once '../resources/portabilityLayer.php';
clearstatcache();
if (file_exists(sys_get_temp_dir() . "/reload-main-resource.tmp"))
    unlink(sys_get_temp_dir() . "/reload-main-resource.tmp");
?>

<body>
<div id="result"></div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function reloadIframe() {
    window.frames[0].location.reload();
}

function finish() {
    document.getElementById("result").innerText = "PASS";
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
<iframe src="resources/reload-main-resource-iframe.php" onload="reloadIframe();"></iframe>
</body>
