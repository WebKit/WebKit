function assert(testID, b) {
    if (!b)
        throw new Error("FAILED test " + testID);
}

function assertEquals(testID, a, b) {
    assert(testID, a == b);
}

var desc = Object.getOwnPropertyDescriptor(Error, "stackTraceLimit");

assertEquals(100, typeof desc.value, "number");
assertEquals(200, desc.writable, true);
assertEquals(300, desc.enumerable, true);
assertEquals(400, desc.configurable, true);
assertEquals(500, desc.get, undefined);
assertEquals(600, desc.set, undefined);

function recurse(x) {
    if (x)
        recurse(x - 1);
    else
        throw Error();
}

function numberOfFrames(str) {
    if (str == "")
        return 0;
    var lines = str.split(/\r\n|\r|\n/);
    // note: Chrome always prints a header line. So, for Chrome, use lines.length - 1.
    return lines.length;
}

var exception = undefined;

function testLimit(testID, updateLimit, reentryCount, expectedLimit, expectedNumberOfFrames) {
    exception = undefined;
    updateLimit();
    assertEquals(testID, Error.stackTraceLimit, expectedLimit);

    try {
        recurse(reentryCount);
    } catch (e) {
        exception = e;
    }

    assertEquals(testID + 1, exception, "Error");
    if (typeof expectedNumberOfFrames == "undefined")
        assertEquals(testID + 2, exception.stack, undefined);
    else
        assertEquals(testID + 3, numberOfFrames(exception.stack), expectedNumberOfFrames);
}

testLimit(1000, () => { Error.stackTraceLimit = 0 }, 1000, 0, 0);
// note: Chrome always prints a header line. So, Chrome expects "Error" here.
assertEquals(1100, exception.stack, "");

testLimit(2000, () => { Error.stackTraceLimit = 10 }, 1000, 10, 10);
testLimit(3000, () => { Error.stackTraceLimit = 100 }, 1000, 100, 100);
testLimit(4000, () => { Error.stackTraceLimit = 1000 }, 1000, 1000, 1000);

// expectedNumberOfFrames includes (1) global + (2) testLimit + (3) 1000 recursion of
// recurse() + (4) recurse() which discovered x == 0 i.e. expectedNumberOfFrames == 1003.
testLimit(5000, () => { Error.stackTraceLimit = 2000 }, 1000, 2000, 1003);

var value = { };
testLimit(6000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = { valueOf() { return 5 } };
testLimit(7000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = [ 1, 2, 3 ];
testLimit(8000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = "hello";
testLimit(9000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = Symbol("hello");
testLimit(10000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = true;
testLimit(11000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = false;
testLimit(12000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

var value = undefined;
testLimit(13000, () => { Error.stackTraceLimit = value }, 1000, value, undefined);

testLimit(14000, () => { Error.stackTraceLimit = 10 }, 1000, 10, 10);

testLimit(15000, () => { delete Error.stackTraceLimit; }, 1000, undefined, undefined);

testLimit(16000, () => { Error.stackTraceLimit = 10 }, 1000, 10, 10);
