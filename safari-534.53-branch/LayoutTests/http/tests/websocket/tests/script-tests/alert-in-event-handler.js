description("Make sure event handler called serially.");

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
}

function endTest()
{
    isSuccessfullyParsed();
    if (window.layoutTestController) {
        layoutTestController.notifyDone();
    }
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/send2");

ws.onopen = function()
{
    debug("Connected");
};

ws.onmessage = function(messageEvent)
{
    debug("Enter onmessage: " + messageEvent.data);
    // alert() will suspend/resume WebSocket.
    alert("message handled." + messageEvent.data);
    debug("Leave onmessage: " + messageEvent.data);
};

ws.onclose = function()
{
    debug("Closed");
    endTest();
};

debug("alert will suspend/resume WebSocket.");
alert("waiting for open");
debug("onopen should fire later.");
var successfullyParsed = true;
