description("This test checks whether orphaned workers exit under various conditions");

if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    debug("NOTE: This test relies on functionality in DumpRenderTree to detect when workers have exited - test results will be incorrect when run in a browser.");
    runTests();
}

// Contains tests for dedicated-worker-specific lifecycle functionality.
function runTests()
{
    // Start a worker, drop/GC our reference to it, make sure it exits.
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (window.layoutTestController) {
            if (layoutTestController.workerThreadCount == 1)
                testPassed("Orphaned worker thread created.");
            else
                testFailed("After thread creation: layoutTestController.workerThreadCount = " + layoutTestController.workerThreadCount);
        }

        // Orphan our worker (no more references to it) and wait for it to exit.
        worker.onmessage = 0;
        worker = 0;
        // Allocating a Date object seems to scramble the stack and force the worker object to get GC'd.
        new Date();
        waitUntilWorkerThreadsExit(orphanedWorkerExited);
    }
}

function orphanedWorkerExited()
{
    testPassed("Orphaned worker thread exited.");
    // Start a worker, drop/GC our reference to it, make sure it exits.
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (window.layoutTestController) {
            if (layoutTestController.workerThreadCount == 1)
                testPassed("Orphaned timeout worker thread created.");
            else
                testFailed("After thread creation: layoutTestController.workerThreadCount = " + layoutTestController.workerThreadCount);
        }
        // Send a message that starts up an async operation, to make sure the thread exits when it completes.
        // FIXME: Disabled for now - re-enable when bug 28702 is fixed.
        //worker.postMessage("eval setTimeout('', 10)");

        // Orphan our worker (no more references to it) and wait for it to exit.
        worker.onmessage = 0;
        worker = 0;
        // For some reason, the worker object does not get GC'd unless we allocate a new object here.
        // The conjecture is that there's a value on the stack that appears to point to the worker which this clobbers.
        new Date();
        waitUntilWorkerThreadsExit(orphanedTimeoutWorkerExited);
    }
}

function orphanedTimeoutWorkerExited()
{
    testPassed("Orphaned timeout worker thread exited.");
    done();
}
