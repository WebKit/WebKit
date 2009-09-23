// Useful utilities for worker tests

function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) { // force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

function waitUntilWorkerThreadsExit(callback)
{
    waitUntilThreadCountMatches(callback, 0);
}

function waitUntilThreadCountMatches(callback, count)
{
    // When running in a browser, just wait for one second then call the callback.
    if (!window.layoutTestController) {
        setTimeout(function() { gc(); callback(); }, 1000);
        return;
    }

    if (layoutTestController.workerThreadCount == count) {
        // Worker threads have exited.
        callback();
    } else {
        // Poll until worker threads have been GC'd/exited.
        gc();
        setTimeout(function() { waitUntilThreadCountMatches(callback, count); }, 10);
    }
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
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }, 0);
}
