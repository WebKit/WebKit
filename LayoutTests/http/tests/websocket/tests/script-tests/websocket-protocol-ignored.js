description("Test WebSocket handshake success without protocol and ignore WebSocket-Protocol from server.");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var protocol;

function endTest()
{
    shouldBe("protocol", '"sub-protocol"');
    clearTimeout(timeoutID);
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var url = "ws://localhost:8880/websocket/tests/protocol-test?protocol=sub-protocol";
var ws = new WebSocket(url);

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
