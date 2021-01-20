var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/protocol-test?protocol=superchat", ["chat", "superchat"]);

if (ws.protocol === "")
    postMessage("PASS: ws.protocol is equal to \"\"");
else
    postMessage("FAIL: ws.protocol should be \"\" but was \"" + ws.protocol + "\"");

ws.onopen = function()
{
    postMessage("INFO: Connected");
    if (ws.protocol === "superchat")
        postMessage("PASS: ws.protocol is equal to \"superchat\"");
    else
        postMessage("FAIL: ws.protocol should be \"superchat\" but was \"" + ws.protocol + "\"");
};

ws.onmessage = function(event)
{
    var receivedMessage = event.data;
    postMessage("INFO: Received: " + receivedMessage);
    if (receivedMessage === "superchat")
        postMessage("PASS: receivedMessage is equal to \"superchat\"");
    else
        postMessage("FAIL: receivedMessage should be \"superchat\" but was \"" + ws.protocol + "\"");
};

ws.onclose = function(closeEvent)
{
    postMessage("INFO: Closed");

    if (ws.protocol === "superchat")
        postMessage("PASS: ws.protocol is equal to \"superchat\"");
    else
        postMessage("FAIL: ws.protocol should be \"superchat\" but was \"" + ws.protocol + "\"");

    if (closeEvent.wasClean === true)
        postMessage("PASS: closeEvent.wasClean is true");
    else
        postMessage("FAIL: closeEvent.wasClean should be true but was \"" + closeEvent.wasClean + "\"");

    setTimeout("checkAfterOnClose()", 0);
};

function checkAfterOnClose()
{
    postMessage("INFO: Exited onclose handler");

    if (ws.protocol === "superchat")
        postMessage("PASS: ws.protocol is equal to \"superchat\"");
    else
        postMessage("FAIL: ws.protocol should be \"superchat\" but was \"" + ws.protocol + "\"");

    postMessage("DONE");
}
