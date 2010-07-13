function runTests()
{
    try {
        var url = 'ws://localhost:8880/websocket/tests/workers/resources/simple';
        var ws = new WebSocket(url);

        ws.onopen = function()
        {
            postMessage('PASS: worker: Connected.');
        };

        ws.onmessage = function(messageEvent)
        {
            postMessage('PASS: worker: Received message: "' + messageEvent.data + '"');
            ws.close();
        };

        ws.onclose = function()
        {
            postMessage('PASS: worker: Closed.');
            postMessage('DONE');
        };
    } catch (e) {
        postMessage('FAIL: worker: Unexpected exception: ' + e);
    } finally {
        postMessage('PASS: worker: Parsed successfully.');
    }
}

runTests();
