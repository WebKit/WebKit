description("This test checks the various use cases around sending multiple ports through WorkerGlobalScope.postMessage");

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var worker = new Worker("resources/worker-context-thread-multi-port.js");

worker.onmessage = function(event) {
    // Report results from worker thread.
    if (event.data == "done")
        event.ports[0].onmessage = done;
    else if (event.data == "noport") {
        if (event.ports && !event.ports.length)
            testPassed("event.ports is non-null and zero length when no port sent");
        else
            testFailed("event.ports is null or non-zero length when no port sent");
    } else if (event.data == "zero ports") {
        if (event.ports && !event.ports.length)
            testPassed("event.ports is non-null and zero length when empty array sent");
        else
            testFailed("event.ports is null or non-zero length when empty array sent");
    } else if (event.data == "two ports") {
        if (!event.ports) {
            testFailed("event.ports should be non-null when ports sent");
            return;
        }
        if (event.ports.length == 2)
            testPassed("event.ports contains two ports when two ports sent");
        else
            testFailed("event.ports contained " + event.ports.length + " when two ports sent");
    } else if (event.data == "failed ports") {
        if (event.ports.length == 2)
            testPassed("event.ports contains two ports when two ports re-sent after error");
        else
            testFailed("event.ports contained " + event.ports.length + " when two ports re-sent after error");
    } else if (event.data.indexOf("PASS") == 0)
        testPassed(event.data.substring(4));
    else if (event.data.indexOf("FAIL") == 0)
        testFailed(event.data.substring(4));
    else
        testFailed("Unexpected result: " + event.data);
}

