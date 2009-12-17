description("Make sure handshake with URL with query components fails against server that doesn't support query component.");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var url = "ws://127.0.0.1:8880/websocket/tests/no-query?";
var handshake_success = false;
var ws_location;

function endTest()
{
    clearTimeout(timeoutID);
    shouldBeFalse("handshake_success");
    shouldBeUndefined("ws_location");
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

debug("url=" + url);
var ws = new WebSocket(url);
ws.onopen = function () {
    debug("WebSocket is open");
    handshake_success = true;
};
ws.onmessge = function (evt) {
    ws_location = evt.data;
    debug("received:" + ws_location);
    ws.close();
};
ws.onclose = function () {
    debug("WebSocket is closed");
    endTest();
};
var timeoutID = setTimeout("endTest()", 2000);

var successfullyParsed = true;
