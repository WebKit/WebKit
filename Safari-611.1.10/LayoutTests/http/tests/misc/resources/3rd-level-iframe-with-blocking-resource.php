<?php
sleep(1);
print "
<html>
<head>
    <script src='notfound.js'></script>
    <style>body { background-color: red; }</style>
</head>
<body>
    PASS.
    <script>
        if (window.testRunner) {
            testRunner.notifyDone();
        }
    </script>
</body>
</html>";
?>
