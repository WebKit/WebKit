description("URL that doesn't have trailing slash should not emit empty Request-URI.");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var url = "ws://127.0.0.1:8880";
var handshake_success = false;
var ws_location;

function endTest()
{
    shouldBeTrue("handshake_success");
    shouldBe("ws_location", '"ws://127.0.0.1:8880/"');
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}


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

var successfullyParsed = true;
