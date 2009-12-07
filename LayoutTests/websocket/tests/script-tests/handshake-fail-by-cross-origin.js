description("Make sure Web Socket connection failed if origin mismatches.");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var connected = false;
var origin;

function endTest()
{
    shouldBeFalse("connected");
    shouldBeUndefined("origin");
    clearTimeout(timeoutID);
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var url = "ws://localhost:8880/websocket/tests/fixed-origin";
debug("document.domain=" + document.domain);
debug("ws.url=" + url);
var ws = new WebSocket(url);

ws.onopen = function()
{
    debug("Connected");
    connected = true;
};

ws.onmessage = function (messageEvent)
{
    origin = messageEvent.data;
    debug("origin=" + origin);
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
