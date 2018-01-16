<?php
   header('Link: <http://localhost:8000>; rel=preconnect');
?>
<!DOCTYPE html>
<html>
<head>
<script src="/js-test-resources/js-test.js"></script>
</head>
<body>
<script>
description("Tests that Link header's rel=preconnect works as expected over HTTP.");
jsTestIsAsync = true;

internals.setConsoleMessageListener(function() {
    finishJSTest();
});
</script>
</body>
</html>
