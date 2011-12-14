var challenge1;
var challenge2;

function endTest()
{
    if (challenge1 === challenge2)
        postMessage('FAIL: worker: challenge1 === challenge2 (challenge was ' + challenge1 + ')');
    else
        postMessage('PASS: worker: challenge1 !== challenge2');

    postMessage('DONE');
}

function runTests()
{
    try {
        var url = 'ws://localhost:8880/websocket/tests/workers/resources/echo-challenge';
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
    } catch (e) {
        postMessage('FAIL: worker: Unexpected exception: ' + e);
    } finally {
        postMessage('PASS: worker: Parsed successfully.');
    }
}

runTests();
