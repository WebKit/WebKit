window.jsTestIsAsync = true;

function startWorker(testScriptURL)
{
    debug('Starting worker: ' + testScriptURL);
    var worker = new Worker(testScriptURL);
    worker.onmessage = function(event)
    {
        if (event.data.length < 5 || event.data.charAt(4) != ':') {
            debug("Got invalid message from worker:" + (event.data ? event.data : "null"));
        } else {
            var code = event.data.substring(0, 4);
            var payload = "[Worker] " + event.data.substring(5);
            if (code == "PASS")
                testPassed(payload);
            else if (code == "FAIL")
                testFailed(payload);
            else if (code == "DESC")
                description(payload);
            else if (code == "DONE")
                finishJSTest();
            else if (code == "MESG")
                debug(payload);
        }
    };

    worker.onerror = function(event)
    {
        debug('Got error from worker: ' + event.message);
        finishJSTest();
    }
}
