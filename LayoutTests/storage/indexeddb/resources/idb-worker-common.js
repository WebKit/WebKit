function debug(message)
{
    postMessage("MESG:" + message);
}

function finishJSTest()
{
    postMessage("DONE:");
}

function description(message)
{
    postMessage('DESC:' + message);
}

function testPassed(msg)
{
    postMessage("PASS:" + msg);
}

function testFailed(msg)
{
    postMessage("FAIL:" + msg);
}
