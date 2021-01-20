function createBlobContainingHelloWorld()
{
    return new Blob(["Hello, world!"]);
}

function createEmptyBlob()
{
    return new Blob([]);
}

function createBlobContainingAllDistinctBytes()
{
    var array = new Uint8Array(256);
    for (var i = 0; i < 256; ++i)
        array[i] = i;
    return new Blob([array]);
}

var url = "ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/check-binary-messages";
var ws = new WebSocket(url);

ws.onopen = function()
{
    ws.send(createBlobContainingHelloWorld());
    ws.send(createEmptyBlob());
    ws.send(createBlobContainingAllDistinctBytes());
};

ws.onmessage = function(event)
{
    postMessage(event.data);
};

ws.onclose = function(closeEvent)
{
    if (closeEvent.wasClean === true)
        postMessage("PASS: closeEvent.wasClean is true.");
    else
        postMessage("FAIL: closeEvent.wasClean should be true, but was: " + closeEvent.wasClean);
    postMessage("DONE");
};
