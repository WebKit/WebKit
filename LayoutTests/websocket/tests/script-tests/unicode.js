description("Web Socket unicode message test");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function endTest()
{
    isSuccessfullyParsed();
    clearTimeout(timeoutID);
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://localhost:8880/websocket/tests/unicode");

// Hello in Japanese
var UNICODE_HELLO = "\u3053\u3093\u306b\u3061\u306f";
// Goodbye in Japanese
var UNICODE_GOODBYE = "\u3055\u3088\u3046\u306a\u3089";

// data needs to be global to be accessbile from shouldBe().
var data = "";

ws.onopen = function()
{
    debug("Connected.");
    debug("Send UNICODE_HELLO.");
    ws.send(UNICODE_HELLO);
};

ws.onmessage = function(messageEvent)
{
    // The server should send back Goodbye if it receives Hello.
    data = messageEvent.data;
    shouldBe("data", "UNICODE_GOODBYE");
};

ws.onclose = function()
{
    debug("Closed.");
    endTest();
};

function timeOutCallback()
{
    testFailed("Timed out in state: " + ws.readyState);
    endTest();
}

var timeoutID = setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
