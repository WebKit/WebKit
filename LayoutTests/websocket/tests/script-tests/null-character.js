description("Make sure WebSocket transfer null character");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function finish()
{
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/echo");
// \xff in string would be \xc3\xbf on websocket connection (UTF-8)
var expectedMessage = "Should Not\xff\0Split";

ws.onopen = function()
{
    debug("WebSocket open");
    ws.send(expectedMessage);
};

var msg;
ws.onmessage = function(messageEvent)
{
    msg = messageEvent.data;
    debug("msg should not be split by frame char \\xff\\0");
    shouldBe("msg", "expectedMessage");
    ws.close();
};

ws.onclose = function()
{
    debug("WebSocket closed");
    finish();
};

var successfullyParsed = true;
