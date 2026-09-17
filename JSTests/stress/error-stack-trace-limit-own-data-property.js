function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

function recurse(x, constructor) {
    if (x)
        return recurse(x - 1, constructor);
    return new constructor();
}

function frameCount(constructor = Error) {
    var stack = recurse(20, constructor).stack;
    if (stack === undefined)
        return undefined;
    return stack.split("\n").length;
}

Error.stackTraceLimit = 10;
shouldBe(frameCount(), 10, "assignment");

Object.defineProperty(Error, "stackTraceLimit", { value: 3 });
shouldBe(frameCount(), 3, "defineProperty");

function captureStackTraceAt(x, object) {
    if (x)
        return captureStackTraceAt(x - 1, object);
    Error.captureStackTrace(object);
    return object;
}
shouldBe(captureStackTraceAt(20, { }).stack.split("\n").length, 3, "defineProperty then captureStackTrace");

Object.defineProperty(Error, "stackTraceLimit", { value: 4 });
shouldBe(frameCount(), 4, "defineProperty again");

TypeError.stackTraceLimit = 2;
shouldBe(Object.hasOwn(TypeError, "stackTraceLimit"), true, "TypeError.stackTraceLimit");
shouldBe(Error.stackTraceLimit, 4, "TypeError.stackTraceLimit");
shouldBe(frameCount(), 4, "TypeError.stackTraceLimit");
shouldBe(frameCount(TypeError), 4, "TypeError.stackTraceLimit");
delete TypeError.stackTraceLimit;

class DerivedError extends Error { }
DerivedError.stackTraceLimit = 2;
shouldBe(Object.hasOwn(DerivedError, "stackTraceLimit"), true, "DerivedError.stackTraceLimit");
shouldBe(frameCount(DerivedError), 4, "DerivedError.stackTraceLimit");

var receiver = { };
shouldBe(Reflect.set(Error, "stackTraceLimit", 2, receiver), true, "Reflect.set with another receiver");
shouldBe(receiver.stackTraceLimit, 2, "Reflect.set with another receiver");
shouldBe(Error.stackTraceLimit, 4, "Reflect.set with another receiver");
shouldBe(frameCount(), 4, "Reflect.set with another receiver");

Object.defineProperty(Error, "stackTraceLimit", { value: 5, writable: false });
Error.stackTraceLimit = 2;
shouldBe(Error.stackTraceLimit, 5, "non-writable");
shouldBe(frameCount(), 5, "non-writable");
Object.defineProperty(Error, "stackTraceLimit", { writable: true });

var getterCalls = 0;
Object.defineProperty(Error, "stackTraceLimit", {
    get() {
        getterCalls++;
        return 6;
    },
    set(value) { },
});
shouldBe(frameCount(), undefined, "accessor");
shouldBe(getterCalls, 0, "accessor");
Error.stackTraceLimit = 2;
shouldBe(frameCount(), undefined, "accessor");

delete Error.stackTraceLimit;
shouldBe(frameCount(), undefined, "deleted");

Error.stackTraceLimit = 7;
shouldBe(frameCount(), 7, "re-added");

var other = createGlobalObject();
other.Error.stackTraceLimit = 2;
shouldBe(frameCount(), 7, "other realm");
shouldBe(frameCount(other.Error), 2, "other realm");
shouldBe(frameCount(other.TypeError), 2, "other realm");

Object.freeze(Error);
Error.stackTraceLimit = 2;
shouldBe(Error.stackTraceLimit, 7, "frozen");
shouldBe(frameCount(), 7, "frozen");
