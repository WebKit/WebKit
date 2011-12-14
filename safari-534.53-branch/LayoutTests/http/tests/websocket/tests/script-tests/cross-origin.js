description("Web Socket Cross Origin test");

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var origin;

function endTest()
{
    shouldBe("origin", '"http://127.0.0.1:8000"');
    clearTimeout(timeoutID);
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var url = "ws://localhost:8880/websocket/tests/origin-test";
debug("document.domain=" + document.domain);
debug("ws.url=" + url);
var ws = new WebSocket(url);

ws.onopen = function()
{
    debug("Connected");
};

ws.onmessage = function (messageEvent)
{
    origin = messageEvent.data;
    ws.close();
};

ws.onclose = function()
{
    endTest();
};

function timeOutCallback()
{
    debug("Timed out in state: " + ws.readyState);
    endTest();
}

var timeoutID = setTimeout(timeOutCallback, 3000);

var successfullyParsed = true;
