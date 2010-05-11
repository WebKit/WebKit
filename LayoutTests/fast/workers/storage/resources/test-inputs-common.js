function log(message)
{
    document.getElementById("console").innerHTML += message + "<br>";
}

function finishTest()
{
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

function runTest(workerJSFile)
{
    if (!workerJSFile) {
        log("FAIL: no JS file specified for the worker.");
    }

    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
    }

    var worker = new Worker(workerJSFile);
    worker.onmessage = function(event) {
        if (event.data == "done")
            finishTest();
        else
            log(event.data);
    }
}
