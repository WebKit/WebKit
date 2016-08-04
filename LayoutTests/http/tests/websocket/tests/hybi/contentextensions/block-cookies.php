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

function clearCookies()
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "block-cookies.php?clear=1", false);
    xhr.send(null);
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/contentextensions/resources/echo-cookie");
ws.onopen = function() {
    debug("WebSocket open");
};
ws.onmessage = function(evt) {
    debug("WebSocket message (" + evt.data + ")");
    clearCookies();
    finishJSTest();
};

</script>
<script src="../../../../../js-test-resources/js-test-post.js"></script>
</body>
</html>
