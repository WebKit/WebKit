//@ runFTLNoCJIT

function shouldEqual(testId, actual, expected) {
    if (actual != expected) {
        throw testId + ": ERROR: expect " + expected + ", actual " + actual;
    }
}

let numberOfIterations = 10000;

function testInvokeGetter() {
    var getter = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").get;
    return (function() { return getter(); })();
}
noInline(testInvokeGetter);

function testInvokeSetter() {
    var setter = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").set;
    return (function() { return setter({}); })();
}
noInline(testInvokeSetter);

function runTest(testId, test, expectedResult, expectedException) {
    for (var i = 0; i < numberOfIterations; i++) {
        var exception;
        var result;
        try {
            result = test({});
        } catch (e) {
            exception = "" + e;
        }
        shouldEqual(testId, result, expectedResult);
        shouldEqual(testId, exception, expectedException);
    }
}

runTest(10000, testInvokeGetter, undefined, "TypeError: Object.prototype.__proto__ called on null or undefined");
runTest(10100, testInvokeSetter, undefined, "TypeError: Object.prototype.__proto__ called on null or undefined");
