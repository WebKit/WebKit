description("Make sure WebSocket gives errors on long invalid upgrade header.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function finish() {
    clearTimeout(timeoutID);

    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/long-invalid-header");
ws.onopen = function () {
    debug("WebSocket is open");
};
ws.onclose = function () {
    debug("WebSocket is closed");
    finish();
};
var timeoutID = setTimeout("finish()", 2000);

var successfullyParsed = true;
