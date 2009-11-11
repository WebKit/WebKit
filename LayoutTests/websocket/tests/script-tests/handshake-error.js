description("Handshake error test");

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
}

function endTest()
{
    isSuccessfullyParsed();
    if (window.layoutTestController) {
        layoutTestController.notifyDone();
    }
}

var ws = new WebSocket("ws://localhost:8880/websocket/tests/handshake-error");

ws.onopen = function()
{
    testFailed("Unexpectedly Connected.");
};

ws.onmessage = function(messageEvent)
{
    testFailed("Unexpectedly Received: '" + messageEvent.data + "'");
};

ws.onclose = function()
{
    debug("Closed.");
    shouldBe("ws.readyState", "2")
    endTest();
};

function timeOutCallback()
{
    testFailed("Timed out in state: " + ws.readyState);
    endTest();
}

window.setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
