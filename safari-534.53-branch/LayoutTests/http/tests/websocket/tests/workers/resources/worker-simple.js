if (self.postMessage)
    runTests();
else
    onconnect = handleConnect;

function handleConnect(event)
{
    // For shared workers, create a faux postMessage() API to send message back to the parent page.
    self.postMessage = function (message) { event.ports[0].postMessage(message); };
    runTests();
};

function runTests()
{
    var ws;
    try {
        postMessage("PASS: worker: init");
        if ('WebSocket' in self)
            postMessage("PASS: worker: WebSocket exists");
        else
            postMessage("PASS: worker: no WebSocket");
        ws = new WebSocket('ws://localhost:8880/websocket/tests/workers/resources/simple');
        ws.onopen = function() {
            postMessage("PASS: worker: Connected.");
        };
        ws.onmessage = function(evt) {
            postMessage("PASS: worker: Received: '" + evt.data + "'");
        };
        ws.onclose = function() {
            postMessage("PASS: worker: Closed.");
            postMessage("DONE");
        };
    } catch (e) {
        postMessage("FAIL: worker: Unexpected exception: " + e);
    } finally {
        postMessage("PASS: worker: successfullyParsed:" + ws);
    }
};
