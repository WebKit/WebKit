<?php
if ( isset($_GET['clear'])) {
    header("Content-Type: text/plain");
    setcookie("WK-websocket-test", "0", time()-1);
    setcookie("WK-websocket-test-httponly", "0", time()-1, "", "", false, true);
    echo("Cookies are cleared.");
    return;
} else {
    header("Content-Type: text/html");
    setcookie("WK-websocket-test", "1");
    setcookie("WK-websocket-test-httponly", "1", time()+3600, "", "", false, true);
    header("Set-Cookie: WK-websocket-test=1");
    header("Set-Cookie: WK-websocket-test-httponly=1; HttpOnly");
}
?>
<html>
<head>
<script src="../../../../../js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Tests that WebSocket sends no cookies when they are blocked.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

function startsWith(str, prefix) {
    return str.indexOf(prefix) == 0;
}

var worker = new Worker("resources/block-cookies-worker.js");
worker.onmessage = function (event)
{
    var message = event.data;
    debug(message);
    if (startsWith(message, "PASS") || startsWith(message, "FAIL"))
		finishJSTest();
};

</script>
<script src="../../../../../js-test-resources/js-test-post.js"></script>
</body>
</html>
