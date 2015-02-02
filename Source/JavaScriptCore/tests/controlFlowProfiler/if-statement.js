load("./driver/driver.js");

var a, b, c, d;

function testIf(x) {
    if (x > 10 && x < 20) {
        return a;
    } else if (x > 20 && x < 30) {
        return b;
    } else if (x > 30 && x < 40) {
        return c;
    } else {
        return d;
    }

    return null;
}

function noMatches(x) {
    if (x > 10 && x < 20) {
        return a;
    } else if (x > 20 && x < 30) {
        return b;
    } else {
        return c;
    }
}

assert(!hasBasicBlockExecuted(testIf, "return a"), "should not have executed yet.");
assert(!hasBasicBlockExecuted(testIf, "return b"), "should not have executed yet.");
assert(!hasBasicBlockExecuted(testIf, "return c"), "should not have executed yet.");
assert(!hasBasicBlockExecuted(testIf, "return d"), "should not have executed yet.");

testIf(11);
assert(hasBasicBlockExecuted(testIf, "return a"), "should have executed.");
assert(hasBasicBlockExecuted(testIf, "x > 10"), "should have executed.");
assert(!hasBasicBlockExecuted(testIf, "return b"), "should not have executed yet.");

testIf(21);
assert(hasBasicBlockExecuted(testIf, "return b"), "should have executed.");
assert(!hasBasicBlockExecuted(testIf, "return c"), "should not have executed yet.");

testIf(31);
assert(hasBasicBlockExecuted(testIf, "return c"), "should have executed.");
assert(!hasBasicBlockExecuted(testIf, "return d"), "should not have executed yet.");

testIf(0);
assert(hasBasicBlockExecuted(testIf, "return d"), "should have executed.");


noMatches(0);
assert(!hasBasicBlockExecuted(noMatches, "return a"), "should not have executed yet.");
assert(hasBasicBlockExecuted(noMatches, "x > 10"), "should have executed.");
assert(!hasBasicBlockExecuted(noMatches, "return b"), "should not have executed yet.");
assert(hasBasicBlockExecuted(noMatches, "x > 20"), "should have executed.");
assert(hasBasicBlockExecuted(noMatches, "return c"), "should have executed.");
