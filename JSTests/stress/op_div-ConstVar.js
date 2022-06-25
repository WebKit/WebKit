//@ skip if $architecture == "x86"
//@ runFTLNoCJIT("--timeoutMultiplier=2.0")

// If all goes well, this test module will terminate silently. If not, it will print
// errors. See binary-op-test.js for debugging options if needed.

load("./resources/binary-op-test.js", "caller relative");

//============================================================================
// Test configuration data:

var opName = "div";
var op = "/";

load("./resources/binary-op-values.js", "caller relative");

tests = [];
generateBinaryTests(tests, opName, op, "ConstVar", values, values);

run();
