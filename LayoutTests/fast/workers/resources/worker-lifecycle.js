
if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    log("NOTE: Test relies on window.layoutTestController to detect when worker threads have exited. Please run this test via DumpRenderTree");
    waitUntilWorkerThreadsExit(runTests);
}

function runTests()
{
    if (window.layoutTestController)
        log("PASS: workerThreadCount = " + layoutTestController.workerThreadCount);
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (window.layoutTestController) {
            if (layoutTestController.workerThreadCount == 1)
                log("PASS: Worker thread created");
            else
                log("FAIL: After thread creation: layoutTestController.workerThreadCount = " + layoutTestController.workerThreadCount);
        }

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
