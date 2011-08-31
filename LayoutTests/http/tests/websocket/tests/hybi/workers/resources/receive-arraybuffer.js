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

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/workers/resources/binary-frames");
ws.binaryType = "arraybuffer";
if (ws.binaryType === "arraybuffer")
    postMessage("PASS: ws.binaryType is \"arraybuffer\"");
else
    postMessage("FAIL: ws.binaryType should be \"arraybuffer\" but was \"" + ws.binaryType + "\"");

var receivedMessages = [];
var expectedValues = [createArrayBufferContainingHelloWorld(), createEmptyArrayBuffer(), createArrayBufferContainingAllDistinctBytes()];

ws.onmessage = function(event)
{
    receivedMessages.push(event.data);
};

ws.onclose = function(closeEvent)
{
    if (receivedMessages.length === expectedValues.length)
        postMessage("PASS: receivedMessages.length is " + expectedValues.length);
    else
        postMessage("FAIL: receivedMessages.length should be " + expectedValues.length + " but was " + receivedMessages.length);
    for (var i = 0; i < expectedValues.length; ++i)
        check(i);
    postMessage("DONE");
};

function check(index)
{
    postMessage("INFO: Checking message #" + index + ".");
    var responseType = '' + receivedMessages[index];
    if (responseType === "[object ArrayBuffer]")
        postMessage("PASS: responseType is \"[object ArrayBuffer]\"");
    else
        postMessage("FAIL: responseType should be \"[object ArrayBuffer]\" but was \"" + responseType + "\"");
    checkArrayBuffer(index, receivedMessages[index], expectedValues[index]);
}

function checkArrayBuffer(testIndex, actual, expected)
{
    var actualArray = new Uint8Array(actual);
    var expectedArray = new Uint8Array(expected);
    if (actualArray.length === expectedArray.length)
        postMessage("PASS: actualArray.length is " + expectedArray.length);
    else
        postMessage("FAIL: actualArray.length should be " + expectedArray.length + " but was " + actualArray.length);
    // Print only the first mismatched byte in order not to flood console.
    for (var i = 0; i < expectedArray.length; ++i) {
        if (actualArray[i] != expectedArray[i]) {
            postMessage("FAIL: Value mismatch: actualArray[" + i + "] = " + actualArray[i] + ", expectedArray[" + i + "] = " + expectedArray[i]);
            return;
        }
    }
    postMessage("PASS: Passed: Message #" + testIndex + ".");
}
