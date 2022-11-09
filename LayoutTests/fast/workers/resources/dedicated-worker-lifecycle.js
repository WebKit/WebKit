description("This test checks whether orphaned workers exit under various conditions");

// We create a large number of Workers and expect the GC to keep up and
// reclaim a reasonable number of them in a timely manner so that the user
// experience is not impacted.
//
// For this test, "a timely manner" means that we have an expectation that
// before the test times out, we expect that no more than
// maxRemainingOrphanedWorkers of the numberOfWorkers we created will survive
// the GC.
//
// We use maxRemainingOrphanedWorkers as an approximation of the tolerable
// amount of garbage in the system that won't impact the user experience.

var numberOfWorkers = 100;
var maxRemainingOrphanedWorkers = 10;
var workers = [ ];
var numberOfCreatedWorkers = 0;

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    debug("NOTE: This test relies on functionality in DumpRenderTree to detect when workers have exited - test results will be incorrect when run in a browser.");
    runTests();
}

function orphanAllWorkers(callback)
{
    var i;
    for (i = 0; i < numberOfWorkers; i++) {
        workers[i].terminate();
        workers[i] = 0;
    }
    workers = [ ];
    waitUntilThreadCountDoesNotExceed(callback, maxRemainingOrphanedWorkers);
}

// Contains tests for dedicated-worker-specific lifecycle functionality.
function runTests()
{
    var i;
    for (i = 0; i < numberOfWorkers; i++) {
        var worker = createWorker();
        workers[i] = worker;

        worker.postMessage("ping");
        worker.onmessage = function(event) {
            if (internals.workerThreadCount == numberOfWorkers) {
                testPassed("Orphaned worker thread created.");
                orphanAllWorkers(orphanedWorkerExited);
            }
        }
    }
}

function orphanedWorkerExited()
{
    testPassed("Orphaned worker thread exited.");

    var i;
    for (i = 0; i < numberOfWorkers; i++) {
        var worker = createWorker();
        workers[i] = worker;

        worker.postMessage("ping");
        worker.onmessage = function(event) {
            // Send a message that starts up an async operation, to make sure the thread exits when it completes.
            // FIXME: Disabled for now - re-enable when bug 28702 is fixed.
            // worker.postMessage("eval setTimeout('', 10)");

            if (internals.workerThreadCount >= numberOfWorkers) {
                testPassed("Orphaned timeout worker thread created.");
                orphanAllWorkers(orphanedTimeoutWorkerExited);
            }
        }
    }
}

function orphanedTimeoutWorkerExited()
{
    testPassed("Orphaned timeout worker thread exited.");
    done();
}
