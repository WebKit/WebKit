function log(message)
{
    document.getElementById("console").innerHTML += message + "<br>";
}

function finishTest()
{
    if (window.testRunner)
        testRunner.notifyDone();
}

function runTest(workerJSFile)
{
    if (!workerJSFile) {
        log("FAIL: no JS file specified for the worker.");
    }

    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    var worker = new Worker(workerJSFile);
    worker.onmessage = function(event) {
        if (event.data == "done")
            finishTest();
        else
            log(event.data);
    }
}
