description("Simple Web Socket test");

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

var ws = new WebSocket("ws://localhost:8880/websocket/tests/simple");

ws.onopen = function()
{
    debug("Connected.");
};

ws.onmessage = function(messageEvent)
{
    debug("Received: '" + messageEvent.data + "'");
};

ws.onclose = function()
{
    debug("Closed.");
    endTest();
};

function timeOutCallback()
{
    debug("Timed out in state: " + ws.readyState);
    endTest();
}

window.setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
