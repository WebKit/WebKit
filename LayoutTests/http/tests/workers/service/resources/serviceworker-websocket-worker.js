async function doTest(event)
{
    if (!event.data.startsWith("WEBSOCKET")) {
        event.source.postMessage("FAIL: received unexpected message from client");
        return;
    }
            event.source.postMessage("PASS");

    try {
        var webSocket = new WebSocket('ws://localhost:8880/websocket/tests/hybi/workers/resources/echo');

        webSocket.onerror = (e) => {
            event.source.postMessage("FAIL: websocket had an error: " + e);
        };

        webSocket.onopen = () => {
            event.source.postMessage("PASS");
        };
    } catch (e) {
        event.source.postMessage("FAIL: exception was raised: " + e);
    }
}

self.addEventListener("message", doTest);
