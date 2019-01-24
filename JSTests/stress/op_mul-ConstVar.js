// FIXME: unskip when this is solved
// https://bugs.webkit.org/show_bug.cgi?id=191163
//@ skip if ["arm", "mips", "x86"].include?($architecture)
//@ runFTLNoCJIT

// If all goes well, this test module will terminate silently. If not, it will print
// errors. See binary-op-test.js for debugging options if needed.

load("./resources/binary-op-test.js");

//============================================================================
// Test configuration data:

var opName = "mul";
var op = "*";

load("./resources/binary-op-values.js");

tests = [];
generateBinaryTests(tests, opName, op, "ConstVar", values, values);

run();
