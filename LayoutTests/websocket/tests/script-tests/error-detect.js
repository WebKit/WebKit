description("Make sure WebSocket correctly fire error event for unknown frame type.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var errorCount = 0;

function finish() {
    shouldBe("errorCount", "255");
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/unknown-frame-type");
ws.onopen = function () {
    debug("WebSocket is open");
};
ws.onmessage = function (evt) {
    debug("received:" + evt.data);
};
ws.onerror = function () {
    errorCount += 1;
};
ws.onclose = function () {
    debug("WebSocket is closed");
    finish();
};

var successfullyParsed = true;
