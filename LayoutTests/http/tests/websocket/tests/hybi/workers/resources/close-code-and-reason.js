var codeNormalClosure = 1000;
var codeNoStatusRcvd = 1005;
var codeAbnormalClosure = 1006;
var emptyString = "";

function postResult(result, actual, expected)
{
    var message = result ? "PASS" : "FAIL";
    message += ": worker: " + actual + " is ";
    if (!result)
        message += "not ";
    message += expected;
    postMessage(message);
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/echo");

ws.onopen = function(event)
{
    testFailed("FAIL: ws.onopen() was called. (message = \"" + event.data + "\")");
};

ws.onclose = function(event)
{
    postMessage("ws.onclose() was called.");
    postResult(!event.wasClean, "event.wasClean", "false");
    postResult(event.code == codeAbnormalClosure, "event.code", "codeAbnormalClosure");
    postResult(event.reason == emptyString, "event.reason", "emptyString");
};

ws.close();

var testId = 0;
var testNum = 7;
var sendData = [
    "-", // request close frame without code and reason
    "--", // request close frame with invalid body which size is 1
    "1000 ok", // request close frame with code 1000 and reason
    "1005 foo", // request close frame with forbidden code 1005 and reason
    "1006 bar", // request close frame with forbidden code 1006 and reason
    "1015 baz", // request close frame with forbidden code 1015 and reason
    "65535 good bye", // request close frame with specified code and reason
];
var expectedCode = [
    codeNoStatusRcvd,
    codeAbnormalClosure,
    codeNormalClosure,
    codeAbnormalClosure,
    codeAbnormalClosure,
    codeAbnormalClosure,
    65535,
];
var expectedReason = [
    "",
    "",
    "ok",
    "",
    "",
    "",
    "good bye",
];
var expectedWasClean = [
    true,
    false,
    true,
    false,
    false,
    false,
    true,
];

WebSocketTest = function() {
    this.ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/close-code-and-reason");
    this.ws.onopen = this.onopen;
    this.ws.onmessage = this.onmessage;
    this.ws.onclose = this.onclose;
};

WebSocketTest.prototype.onopen = function()
{
    postMessage("WebSocketTest.onopen() was called with testId = " + testId + ".");
    this.send(sendData[testId]);
};

WebSocketTest.prototype.onmessage = function(event)
{
    postMessage("FAIL: WebSocketTest.onmessage() was called. (message = \"" + event.data + "\")");
};

WebSocketTest.prototype.onclose = function(event)
{
    postMessage("WebSocketTest.onclose() was called with testId = " + testId + ".");
    postResult(event.wasClean == expectedWasClean[testId], "event.wasClean", expectedWasClean[testId]);
    postResult(event.code == expectedCode[testId], "event.code", expectedCode[testId]);
    postResult(event.reason == expectedReason[testId], "event.reason", expectedReason[testId]);
    event.code = 0;
    event.reason = "readonly";
    event.wasClean = !event.wasClean;
    postResult(event.wasClean == expectedWasClean[testId], "event.wasClean", expectedWasClean[testId]);
    postResult(event.code == expectedCode[testId], "event.code", expectedCode[testId]);
    postResult(event.reason == expectedReason[testId], "event.reason", expectedReason[testId]);
    testId++;
    if (testId < testNum)
        test = new WebSocketTest();
    else
        postMessage("DONE")
};

var test = new WebSocketTest();
