description("This test checks the various use cases around sending multiple ports through Worker.postMessage");

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var channel = new MessageChannel();
var channel2 = new MessageChannel();
var channel3 = new MessageChannel();
var channel4 = new MessageChannel();

var worker = new Worker("resources/worker-thread-multi-port.js");
worker.postMessage("noport");
worker.postMessage("zero ports", []);
worker.postMessage("two ports", [channel2.port1, channel2.port2]);

// Now test various failure cases
shouldThrow('worker.postMessage("null port", [channel3.port1, null, channel3.port2])');
shouldThrow('worker.postMessage("notAPort", [channel3.port1, {}, channel3.port2])');
// Should be OK to send channel3.port1/2 (should not have been disentangled by the previous failed calls).
worker.postMessage("failed ports", [channel3.port1, channel3.port2]);

shouldThrow('worker.postMessage("notAnArray", channel3.port1)')
shouldThrow('worker.postMessage("notASequence", [{length: 3}])');

worker.postMessage("done", [channel.port2]);
worker.onmessage = function(event) {
    // Report results from worker thread.
    if (event.data == "done")
        channel.port1.onmessage = done;
    else if (event.data.indexOf("PASS") == 0)
        testPassed(event.data.substring(4));
    else if (event.data.indexOf("FAIL") == 0)
        testFailed(event.data.substring(4));
    else
        testFailed("Unexpected result: " + event.data);
}

