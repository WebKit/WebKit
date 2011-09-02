function startsWith(target, prefix)
{
    return target.indexOf(prefix) === 0;
}

function createBlobContainingHelloWorld()
{
    var builder = new WebKitBlobBuilder();
    builder.append("Hello, world!");
    return builder.getBlob();
}

function createEmptyBlob()
{
    var builder = new WebKitBlobBuilder();
    return builder.getBlob();
}

function createBlobContainingAllDistinctBytes()
{
    var array = new Uint8Array(256);
    for (var i = 0; i < 256; ++i)
        array[i] = i;
    var builder = new WebKitBlobBuilder();
    builder.append(array.buffer);
    return builder.getBlob();
}

var url = "ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/send-blob";
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
