description("Web Socket send test");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function endTest()
{
    isSuccessfullyParsed();
    clearTimeout(timeoutID);
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://localhost:8880/websocket/tests/send");

var FIRST_MESSAGE_TO_SEND = {
    toString: function() { throw "Pickles"; }
};
// data needs to be global to be accessbile from shouldBe().
var data = "";

ws.onopen = function()
{
    debug("Connected.");
    try {
        ws.send(FIRST_MESSAGE_TO_SEND);
    } catch (ex) {
        debug("Caught exception: " + ex);
    }
    ws.close();
};

ws.onclose = function()
{
    debug("Closed.");
    endTest();
};

function timeOutCallback()
{
    testFailed("Timed out in state: " + ws.readyState);
    endTest();
}

var timeoutID = setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
