description('Handshake request should contain random challenge values.');

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var challenge1;
var challenge2;

function endTest()
{
    shouldBeFalse('challenge1 === challenge2');
    if (challenge1 === challenge2)
        debug('challenge was ' + challenge1);

    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

var url = 'ws://localhost:8880/websocket/tests/echo-challenge';
var ws1 = new WebSocket(url);

ws1.onmessage = function(messageEvent)
{
    challenge1 = messageEvent.data;
    ws1.close();
}

ws1.onclose = function()
{
    var ws2 = new WebSocket(url);

    ws2.onmessage = function(messageEvent)
    {
        challenge2 = messageEvent.data;
        ws2.close();
    }

    ws2.onclose = function()
    {
        endTest();
    }
}

var successfullyParsed = true;
