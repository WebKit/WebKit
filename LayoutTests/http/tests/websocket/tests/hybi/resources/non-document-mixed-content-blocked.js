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
    var ws = new WebSocket('ws://127.0.0.1:8880/websocket/tests/hybi/simple');

    ws.onopen = function() {
        postMessage("FAIL: worker: Connected. readyState " + ws.readyState);
        postMessage("DONE");
    };
    ws.onerror = function(error) {
        postMessage("PASS worker onerror. readyState " + ws.readyState);
        postMessage("DONE");
    };

    setTimeout(function() {
        postMessage("FAIL worker timeout");
    }, 3000);
}
