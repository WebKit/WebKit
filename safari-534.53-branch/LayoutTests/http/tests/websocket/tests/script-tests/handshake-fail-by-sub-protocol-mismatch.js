description("Test WebSocket handshake fail if sub protocol name mismatches.");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var protocol;

function endTest()
{
    shouldBeUndefined("protocol");
    clearTimeout(timeoutID);
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var url = "ws://localhost:8880/websocket/tests/protocol-test?protocol=available-protocol";
var ws = new WebSocket(url, "requestting-protocol");

ws.onopen = function()
{
    debug("Connected");
};

ws.onmessage = function (messageEvent)
{
    protocol = messageEvent.data;
    ws.close();
};

ws.onclose = function()
{
    endTest();
};

function timeOutCallback()
{
    debug("Timed out in state: " + ws.readyState);
    endTest();
}

var timeoutID = setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
