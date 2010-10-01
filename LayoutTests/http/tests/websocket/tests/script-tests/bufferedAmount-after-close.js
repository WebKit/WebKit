description("Web Socket bufferedAmount after closed");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function endTest()
{
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var ws = new WebSocket("ws://localhost:8880/websocket/tests/simple");

ws.onopen = function()
{
    debug("Connected.");
    ws.close();
};

ws.onclose = function()
{
    debug("Closed.");
    shouldBe("ws.readyState", "2");
    shouldBe("ws.bufferedAmount", "0");
    shouldBeFalse("ws.send('send to closed socket')");
    // If the connection is closed, bufferedAmount attribute's value will only
    // increase with each call to the send() method.
    // (the number does not reset to zero once the connection closes).
    shouldBe("ws.bufferedAmount", "23");
    endTest();
};

var successfullyParsed = true;
