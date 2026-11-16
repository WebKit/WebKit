function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual);
}

function recurse(x) {
    if (x)
        recurse(x - 1);
    else
        throw new Error();
}

Error.stackTraceLimit = NaN;
shouldBe(Error.stackTraceLimit, NaN);

var exception;
try {
    recurse(20);
} catch (e) {
    exception = e;
}
shouldBe(exception.stack, undefined);

Error.stackTraceLimit = 5;
shouldBe(Error.stackTraceLimit, 5);
try {
    recurse(20);
} catch (e) {
    exception = e;
}
shouldBe(exception.stack.split(/\r\n|\r|\n/).length, 5);

Error.stackTraceLimit = Infinity;
shouldBe(Error.stackTraceLimit, Infinity);
try {
    recurse(20);
} catch (e) {
    exception = e;
}
if (!(exception.stack.split(/\r\n|\r|\n/).length > 5))
    throw new Error("Infinity should not disable stack traces");

Error.stackTraceLimit = -Infinity;
shouldBe(Error.stackTraceLimit, -Infinity);
try {
    recurse(20);
} catch (e) {
    exception = e;
}
shouldBe(exception.stack, undefined);

Error.stackTraceLimit = -0;
shouldBe(Object.is(Error.stackTraceLimit, -0), true);
try {
    recurse(20);
} catch (e) {
    exception = e;
}
shouldBe(exception.stack, undefined);
