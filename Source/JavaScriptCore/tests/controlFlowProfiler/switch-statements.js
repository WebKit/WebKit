load("./driver/driver.js");

var a, b, c;
function testSwitch(s) {
    switch (s) {
    case "foo":
        return a;
    case "bar":
        return b;
    default:
        return c;
    }
}

assert(!hasBasicBlockExecuted(testSwitch, "switch"), "should not have executed yet.");

testSwitch("foo");
assert(hasBasicBlockExecuted(testSwitch, "switch"), "should have executed.");
assert(hasBasicBlockExecuted(testSwitch, "return a"), "should have executed.");
assert(!hasBasicBlockExecuted(testSwitch, "return b"), "should not have executed yet.");
assert(!hasBasicBlockExecuted(testSwitch, "return c"), "should not have executed yet.");

testSwitch("bar");
assert(hasBasicBlockExecuted(testSwitch, "return b"), "should have executed.");
assert(!hasBasicBlockExecuted(testSwitch, "return c"), "should not have executed yet.");

testSwitch("");
assert(hasBasicBlockExecuted(testSwitch, "return c"), "should have executed.");
