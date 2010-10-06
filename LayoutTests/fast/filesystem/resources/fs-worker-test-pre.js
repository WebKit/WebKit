if (window.layoutTestController)
    layoutTestController.dumpAsText();

function debug(message)
{
    document.getElementById("console").innerHTML += message + "<br/>";
}

function startWorker(testScriptURL)
{
    debug('Starting worker: ' + testScriptURL);
    var worker = new Worker('../resources/fs-worker-common.js');
    worker.postMessage(testScriptURL);
    worker.onmessage = function(event)
    {
        debug(event.data);
        if (event.data == "DONE")
            finishJSTest();
    };

    worker.onerror = function(event)
    {
        alert(event);
        debug('Got error from worker: ' + event.message);
        finishJSTest();
    }
}

function finishJSTest()
{
    wasFinishJSTestCalled = true;
    if (!window.wasPostTestScriptParsed)
        return;
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}
