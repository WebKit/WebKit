function doit()
{
    function callback(result)
    {
        debug("Received console messages:");
        for (var i = 0; i < result.length; ++i) {
            var r = result[i];
            debug("Message[" + i + "]:");
            debug("URL: " + r.url);
            debug("Message: " + r.message);
        }
        notifyDone();
        debug("TEST COMPLETE.");
    }
    evaluateInWebInspector("frontend_dumpConsoleMessages", callback);
}

function receiveMessage(event) {
    if (event.data != "frameReloaded") {
        testFailed("Unexpected message: " + event.data);
        if (window.layoutTestController)
            layoutTestController.notifyDone();
        return;
    }
    if (window.layoutTestController)
        layoutTestController.showWebInspector();
    debug("Showing Web Inspector...");
}

window.addEventListener("message", receiveMessage, false);

// Frontend functions.

function frontend_dumpConsoleMessages()
{
    var result = [];
    var messages = WebInspector.console.messages;
    for (var i = 0; i < messages.length; ++i) {
        var m = messages[i];
        result.push({ message: m.message, url: m.url});
    }
    return result;
}
