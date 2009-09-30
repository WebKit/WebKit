if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

description("This test checks the various use cases around sending multiple ports through MessagePort.postMessage");

var channel = new MessageChannel();
var channel2 = new MessageChannel();
var channel3 = new MessageChannel();

channel.port1.postMessage("noport");
channel.port1.postMessage("zero ports", []);
channel.port1.postMessage("two ports", [channel2.port1, channel2.port2]);

// Now test various failure cases
shouldThrow('channel.port1.postMessage("same port", [channel.port1])');
shouldThrow('channel.port1.postMessage("entangled port", [channel.port2])');
shouldThrow('channel.port1.postMessage("null port", [channel3.port1, null, channel3.port2])');
shouldThrow('channel.port1.postMessage("notAPort", [channel3.port1, {}, channel3.port2])');
// Should be OK to send channel3.port1 (should not have been disentangled by the previous failed calls).
channel.port1.postMessage("entangled ports", [channel3.port1, channel3.port2]);

shouldThrow('channel.port1.postMessage("notAnArray", channel3.port1)')
shouldThrow('channel.port1.postMessage("notASequence", [{length: 3}])');

channel.port1.postMessage("done");

channel.port2.onmessage = function(event) {
    if (event.data == "noport") {
        if (!event.ports)
            testPassed("event.ports is null when no port sent");
        else
            testFailed("event.ports should be null when no port sent");
    } else if (event.data == "zero ports") {
        if (!event.ports)
            testPassed("event.ports is null when empty array sent");
        else
            testFailed("event.ports should be null when empty array sent");
    } else if (event.data == "two ports") {
        if (!event.ports) {
            testFailed("event.ports should be non-null when ports sent");
            return;
        }
        if (event.ports.length == 2)
            testPassed("event.ports contains two ports when two ports sent");
        else
            testFailed("event.ports contained " + event.ports.length + " when two ports sent");

    } else if (event.data == "entangled ports") {
        if (event.ports.length == 2)
            testPassed("event.ports contains two ports when two ports re-sent after error");
        else
            testFailed("event.ports contained " + event.ports.length + " when two ports re-sent after error");
    } else if (event.data == "done") {
        debug('<br><span class="pass">TEST COMPLETE</span>');
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    } else
        testFailed("Received unexpected message: " + event.data);
}

