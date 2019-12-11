var channel = new MessageChannel();
var channel2 = new MessageChannel();
var channel3 = new MessageChannel();

postMessage("noport");
postMessage("zero ports", []);
postMessage("two ports", [channel2.port1, channel2.port2]);

// Now test various failure cases
try {
    postMessage("null port", [channel3.port1, null, channel3.port2]);
    testFailed("posting a null port did not throw");
} catch (e) {
    testPassed("posting a null port did throw: " + e);
}

try {
    postMessage("notAPort", [channel3.port1, {}, channel3.port2]);
    testFailed("posting a non-port did not throw");
} catch (e) {
    testPassed("posting a non-port did throw: " + e);
}

try {
    postMessage("failed ports", [channel3.port1, channel3.port2]);
} catch (e) {
    testFailed("reposting ports threw an exception: " + e);
}

try {
    postMessage("notAnArray", channel3.port1);
    testFailed("posting a non-array should throw");
} catch (e) {
    testPassed("posting a non-array did throw: " + e);
}

try {
    postMessage("notASequence", [{length: 3}]);
    testFailed("posting a non-sequence should throw");
} catch (e) {
   testPassed("posting a non-sequence did throw: " + e);
}


postMessage("done", [channel.port2]);
channel.port1.postMessage("done");


function testFailed(msg) {
    postMessage("FAIL"+msg);
}


function testPassed(msg) {
    postMessage("PASS"+msg);
}

