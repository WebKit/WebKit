description("Make sure handshake with URL with empty query components success.");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var url = "ws://127.0.0.1:8880/websocket/tests/echo-location?";
var handshake_success = false;
var ws_location;

function endTest()
{
    clearTimeout(timeoutID);
    shouldBeTrue("handshake_success");
    shouldBe("ws_location", "url");
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
ws.onmessage = function (evt) {
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
