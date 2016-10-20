function tapAtPointAndNotifyDone(x, y)
{
    tapAtPoint(x, y, function () { testRunner.notifyDone(); })
}

function tapAtPoint(x, y, callback)
{
    if (!window.testRunner || !testRunner.runUIScript)
        return;

    var uiScript = `
    (function() {
        uiController.singleTapAtPoint(${x}, ${y}, function() {
            uiController.uiScriptComplete("Done");
        });
    })();`
    testRunner.runUIScript(uiScript, callback);
}
