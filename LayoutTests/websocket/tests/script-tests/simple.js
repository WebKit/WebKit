description("Simple Web Socket test");

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
}

function endTest()
{
    isSuccessfullyParsed();
    clearTimeout(timeoutID);
    if (window.layoutTestController) {
        layoutTestController.notifyDone();
    }
}

var ws = new WebSocket("ws://localhost:8880/websocket/tests/simple");
debug("Created a socket to '" + ws.URL + "'; readyState " + ws.readyState + ".");

ws.onopen = function()
{
    debug("Connected; readyState " + ((ws.readyState >= 0) ? ">= 0." : ws.readyState));
};

ws.onmessage = function(messageEvent)
{
    debug("Received: '" + messageEvent.data + "'; readyState " + ((ws.readyState >= 1) ? ">= 1." : ws.readyState));
};

ws.onclose = function()
{
    debug("Closed; readyState " + ws.readyState + ".");
    endTest();
};

function timeOutCallback()
{
    debug("Timed out in state: " + ws.readyState);
    endTest();
}

var timeoutID = setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
