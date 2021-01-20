<?
header("Content-Type: text/html; charset=utf-8");
?>
<!-- <?php
# Spam a bunch of As to make sure we blow past any buffers.
print str_repeat("A", 2048);
?> -->
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpResourceResponseMIMETypes();
}
</script>
<script src="non-existant-1"></script>
<?php
ob_flush();
flush();
sleep(1);
?>
<script>
document.write("<!--");
</script>
<img src="preload-slow-target.jpg">
<script>
document.write("-->");
</script>
<script src="non-existant-2"></script>
