var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/no-onmessage-in-sync-op");

var events = [];
// Message receipt should be recorded after bufferedAmount and send() calls.
var expectedEvents = ["Got bufferedAmount: 0",
                      "Got bufferedAmount: 0",
                      "Got bufferedAmount: 0",
                      "Sent message: 1",
                      "Sent message: 2",
                      "Sent message: 3",
                      "Received message: 1",
                      "Received message: 2",
                      "Received message: 3"];

ws.onopen = function()
{
    // After handshake, the server will send three messages immediately, but they should not be received
    // until we exit this event loop.
    postMessage("INFO: Waiting for two seconds to make sure we receive messages from the server.");
    var start = (new Date()).getTime();
    while ((new Date()).getTime() - start < 2000) {}

    var bufferedAmount = ws.bufferedAmount;
    events.push("Got bufferedAmount: " + bufferedAmount);
    bufferedAmount = ws.bufferedAmount;
    events.push("Got bufferedAmount: " + bufferedAmount);
    bufferedAmount = ws.bufferedAmount;
    events.push("Got bufferedAmount: " + bufferedAmount);
    ws.send("1");
    events.push("Sent message: 1");
    ws.send("2");
    events.push("Sent message: 2");
    ws.send("3");
    events.push("Sent message: 3");
};

ws.onmessage = function(messageEvent)
{
    events.push("Received message: " + messageEvent.data);
};

ws.onclose = function(closeEvent)
{
    if (closeEvent.wasClean === true)
        postMessage("PASS: closeEvent.wasClean is true");
    else
        postMessage("FAIL: closeEvent.wasClean should be true but was \"" + closeEvent.wasClean + "\"");

    if (events.length === expectedEvents.length)
        postMessage("PASS: event.length is " + expectedEvents.length);
    else
        postMessage("FAIL: event.length should be " + expectedEvents.length + " but was \"" + events.length + "\"");

    for (var i = 0; i < expectedEvents.length; ++i) {
        if (events[i] === expectedEvents[i])
            postMessage("PASS: events[" + i + "] is \"" + expectedEvents[i] + "\"");
        else
            postMessage("FAIL: events[" + i + "] should be \"" + expectedEvents[i] + "\" but was \"" + events[i] + "\"");
    }

    postMessage("DONE");
};
