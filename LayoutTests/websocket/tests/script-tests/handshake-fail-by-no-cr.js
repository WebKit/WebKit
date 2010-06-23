description('Handshake should fail when the first line does not end with CRLF.');

if (window.layoutTestController)
    layoutTestController.waitUntilDone()

var connected = false;
var origin;

function endTest() {
    shouldBeFalse('connected');
    shouldBeUndefined('origin');
    clearTimeout(timeoutID);
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var url = 'ws://localhost:8880/websocket/tests/handshake-fail-by-no-cr';
var ws = new WebSocket(url);

ws.onopen = function()
{
    debug('Connected');
    connected = true;
}

ws.onmessage = function(messageEvent)
{
    origin = messageEvent.data;
    debug('origin = ' + origin);
    ws.close();
}

ws.onclose = function()
{
    endTest();
}

function timeoutCallback()
{
    debug('Timed out (state = ' + ws.readyState + ')');
    endTest();
}

var timeoutID = setTimeout(timeoutCallback, 3000);

var successfullyParsed = true;
