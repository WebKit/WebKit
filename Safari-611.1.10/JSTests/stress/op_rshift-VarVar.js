//@ skip if $model == "Apple Watch Series 3" # This is test for FTL.
//@ runFTLNoCJIT

// If all goes well, this test module will terminate silently. If not, it will print
// errors. See binary-op-test.js for debugging options if needed.

load("./resources/binary-op-test.js", "caller relative");

//============================================================================
// Test configuration data:

var opName = "rshift";
var op = ">>";

load("./resources/binary-op-values.js", "caller relative");

tests = [];
generateBinaryTests(tests, opName, op, "VarVar", values, values);

run();
