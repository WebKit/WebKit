description("Make sure WebSocket doesn't crash with bad handshake message.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function finish() {
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/bad-handshake-crash");
ws.onopen = function () {
    debug("WebSocket is open");
};
ws.onclose = function () {
    debug("WebSocket is closed");
    finish();
};

var successfullyParsed = true;
