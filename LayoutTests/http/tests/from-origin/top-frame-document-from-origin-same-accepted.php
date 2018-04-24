<?php
header("From-Origin: Same");
?>
<html>
<head>
    <script src="/js-test-resources/js-test.js"></script>
    <script>
        description("Tests that a same-origin top frame document load succeeds if the server blocks cross-origin loads with a 'From-Origin: same' response header.");
        jsTestIsAsync = true;
        testRunner.dumpChildFramesAsText();

        function onloadFired() {
            testPassed("Onload event fired.");
            finishJSTest();
        }
    </script>
</head>
<body onload="onloadFired()">
<h3>The Document</h3>
</body>
</html>
