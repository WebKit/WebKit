function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    log("FAIL: Test relies on window.layoutTestController. Please run this test via DumpRenderTree");
    done();
}

function waitUntilWorkerThreadsExit(callback)
{
    if (layoutTestController.workerThreadCount == 0) {
        // Worker threads have exited.
        callback();
    } else {
        // Poll until worker threads have exited.
        setTimeout(function() { waitUntilWorkerThreadsExit(callback); }, 100);
    }
}

function runTests()
{
    log("PASS: workerThreadCount = " + layoutTestController.workerThreadCount);
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (layoutTestController.workerThreadCount == 1)
            log("PASS: Worker thread created");
        else
            log("FAIL: After thread creation: layoutTestController.workerThreadCount = " + layoutTestController.workerThreadCount);

        // Shutdown the worker.
        worker.postMessage("close");
        waitUntilWorkerThreadsExit(workerExited);
    }
}

function workerExited()
{
    log("PASS: Worker exited when close() called.");
    done();
}

function done()
{
    log("DONE");
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}

