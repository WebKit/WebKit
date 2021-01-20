<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}
</script>
</head>
<body>
<iframe src="<?php echo $_GET['src']; ?>"></iframe>
</body>
</html>
