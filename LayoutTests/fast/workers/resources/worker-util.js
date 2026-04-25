// Useful utilities for worker tests

function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

function gc(forceAlloc)
{
    if (typeof GCController !== "undefined")
        GCController.collect();

    if (typeof GCController == "undefined" || forceAlloc) {
        function gcRec(n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        }
        for (var i = 0; i < 1000; i++)
            gcRec(10)
    }
}

function waitUntilWorkerThreadsExit(callback)
{
    waitUntilThreadCountMatches(callback, 0);
}

function waitUntilThreadCountMatchesCondition(callback, condition)
{
    // When running in a browser, just wait for one second then call the callback.
    if (!window.testRunner) {
        setTimeout(function() { gc(true); callback(); }, 1000);
        return;
    }

    if (condition()) {
        // Worker threads have exited.
        callback();
    } else {
        // Poll until worker threads have been GC'd/exited.
        // Force a GC with a bunch of allocations to try to scramble the stack and force worker objects to be collected.
        gc(true);
        setTimeout(function() { waitUntilThreadCountMatchesCondition(callback, condition); }, 10);
    }
}

function waitUntilThreadCountMatches(callback, count)
{
    waitUntilThreadCountMatchesCondition(callback, function() { return internals.workerThreadCount == count; });
}

function waitUntilThreadCountDoesNotExceed(callback, count)
{
    waitUntilThreadCountMatchesCondition(callback, function() { return internals.workerThreadCount <= count; });
}

function ensureThreadCountMatches(callback, count)
{
    // Just wait until the thread count matches, then wait another 100ms to see if it changes, then fire the callback.
    waitUntilThreadCountMatches(function() {
            setTimeout(function() { waitUntilThreadCountMatches(callback, count); }, 100);
        }, count);
}

function done()
{
    if (window.debug)
        debug('<br><span class="pass">TEST COMPLETE</span>');
    else
        log("DONE");

    // Call notifyDone via the queue so any pending console messages/exceptions are written out first.
    setTimeout(function() {
        if (window.testRunner)
            testRunner.notifyDone();
    }, 0);
}
