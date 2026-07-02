function createArrayBufferContainingHelloWorld()
{
    var hello = "Hello, world!";
    var array = new Uint8Array(hello.length);
    for (var i = 0; i < hello.length; ++i)
        array[i] = hello.charCodeAt(i);
    return array.buffer;
}

function createEmptyArrayBuffer()
{
    return new ArrayBuffer(0);
}

function createArrayBufferContainingAllDistinctBytes()
{
    var array = new Uint8Array(256);
    for (var i = 0; i < 256; ++i)
        array[i] = i;
    return array.buffer;
}

var url = "ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/check-binary-messages";
var ws = new WebSocket(url);

ws.onopen = function()
{
    ws.send(createArrayBufferContainingHelloWorld());
    ws.send(createEmptyArrayBuffer());
    ws.send(createArrayBufferContainingAllDistinctBytes());
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
