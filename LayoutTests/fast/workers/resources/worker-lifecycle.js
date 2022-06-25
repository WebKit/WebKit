
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    log("NOTE: Test relies on window.testRunner to detect when worker threads have exited. Please run this test via DumpRenderTree");
    waitUntilWorkerThreadsExit(runTests);
}

function runTests()
{
    if (window.testRunner)
        log("PASS: workerThreadCount = " + internals.workerThreadCount);
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (window.testRunner) {
            if (internals.workerThreadCount == 1)
                log("PASS: Worker thread created");
            else
                log("FAIL: After thread creation: internals.workerThreadCount = " + internals.workerThreadCount);
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
