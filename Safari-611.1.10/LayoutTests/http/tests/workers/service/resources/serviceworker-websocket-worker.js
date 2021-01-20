async function doTest(event)
{
    if (!event.data.startsWith("WEBSOCKET")) {
        event.source.postMessage("FAIL: received unexpected message from client");
        return;
    }

    try {
        var webSocket = new WebSocket('wss://localhost:9323/websocket/tests/hybi/workers/resources/echo');
        webSocket.onerror = (e) => {
            event.source.postMessage("FAIL: websocket had an error: " + e);
        };

        webSocket.onmessage = (e) => {
            event.source.postMessage("PASS");
        };

        webSocket.onopen = () => {
            webSocket.send("PASS?");
        };
    } catch (e) {
        event.source.postMessage("FAIL: exception was raised: " + e);
    }
}

self.addEventListener("message", doTest);
