function createArrayBufferViewContainingHelloWorld()
{
    var hello = "Hello, world!";
    var array = new Uint8Array(hello.length);
    for (var i = 0; i < hello.length; ++i)
        array[i] = hello.charCodeAt(i);
    return array;
}

function createEmptyArrayBufferView()
{
    return new Uint8Array(0);
}

function createArrayBufferViewContainingAllDistinctBytes()
{
    // Return a slice of ArrayBuffer.
    var buffer = new ArrayBuffer(1000);
    var array = new Uint8Array(buffer, 123, 256);
    for (var i = 0; i < 256; ++i)
        array[i] = i;
    return array;
}

var url = "ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/check-binary-messages";
var ws = new WebSocket(url);

ws.onopen = function()
{
    ws.send(createArrayBufferViewContainingHelloWorld());
    ws.send(createEmptyArrayBufferView());
    ws.send(createArrayBufferViewContainingAllDistinctBytes());
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
