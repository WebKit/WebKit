description("Make sure WebSocket correctly skip lengthed frame.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var received_messages = [];
var expected_messages = ["hello", "world"];
function finish() {
    debug(received_messages.length);
    for (var i = 0; i < received_messages; i++) {
        debug("received[" + i + "]=" + received_messages[i]);
    }

    shouldBeTrue("areArraysEqual(received_messages, expected_messages)");

    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://localhost:8880/websocket/tests/frame-length-skip");
ws.onopen = function () {
    debug("WebSocket is open");
};
ws.onmessage = function (evt) {
    debug("received:" + evt.data);
    received_messages.push(evt.data);
};
ws.close = function () {
    debug("WebSocket is closed");
    finish();
};
setTimeout("finish()", 2000);

var successfullyParsed = true;
