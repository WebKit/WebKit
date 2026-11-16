function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

function recurse(x) {
    if (x)
        return recurse(x - 1);
    return new Error();
}
noInline(recurse);

function frameCount(error) {
    return error.stack.split("\n").length;
}

function shouldCheck(i) {
    return !(i % 61) || i + 20 >= testLoopCount;
}

function readLimit() {
    return Error.stackTraceLimit;
}
noInline(readLimit);

function setLimit(limit) {
    Error.stackTraceLimit = limit;
}
noInline(setLimit);

function setLimitAndCapture(limit) {
    Error.stackTraceLimit = limit;
    return recurse(10);
}
noInline(setLimitAndCapture);

for (var i = 0; i < testLoopCount; ++i)
    readLimit();

for (var i = 0; i < testLoopCount; ++i) {
    var limit = 1 + (i % 7);
    Error.stackTraceLimit = limit;
    if (shouldCheck(i))
        shouldBe(frameCount(recurse(10)), limit, "global code, iteration " + i);
}

for (var i = 0; i < testLoopCount; ++i) {
    var limit = 1 + (i % 7);
    setLimit(limit);
    if (shouldCheck(i)) {
        shouldBe(readLimit(), limit, "setLimit, iteration " + i);
        shouldBe(frameCount(recurse(10)), limit, "setLimit, iteration " + i);
    }
}

for (var i = 0; i < testLoopCount; ++i) {
    var limit = 1 + (i % 7);
    var error = setLimitAndCapture(limit);
    if (shouldCheck(i))
        shouldBe(frameCount(error), limit, "setLimitAndCapture, iteration " + i);
}

setLimit(0);
shouldBe(recurse(10).stack, undefined, "limit 0");

setLimit("not a number");
shouldBe(recurse(10).stack, undefined, "non-number limit");

setLimit(3);
shouldBe(frameCount(recurse(10)), 3, "limit restored");
