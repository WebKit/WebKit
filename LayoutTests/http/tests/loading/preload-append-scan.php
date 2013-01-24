<?
header("Content-Type: text/html; charset=utf-8");
?>
<!-- <?php
# Spam a bunch of As to make sure we blow past any buffers.
print str_repeat("A", 2048);
?> -->
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();

function checkForPreload() {
    var result;
    if (internals.isPreloaded("resources/preload-test.jpg"))
        result = "PASS";
    else
        result = "FAIL";
    document.getElementsByTagName("body")[0].appendChild(document.createTextNode(result));
}

window.addEventListener("DOMContentLoaded", checkForPreload, false);

function debug(x) {}
</script>
<script src="http://127.0.0.1:8000/resources/slow-script.pl?delay=1000"></script>
<?php
flush();
usleep(200000);
?>
<script>
document.write("<plaintext>");
</script>
This test needs to be run in DRT. Preload scanner should see the image resource.
<img src="resources/preload-test.jpg">
