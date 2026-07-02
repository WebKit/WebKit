//@ runFTLNoCJIT

function shouldEqual(testId, actual, expected) {
    if (actual != expected) {
        throw testId + ": ERROR: expect " + expected + ", actual " + actual;
    }
}

function frozenArrayReviver(k, v) {
    if (k === "a") {
        this.b = Object.freeze(["unmodifiable"]);
        return v;
    }
    if (k === "0")
        return "modified";
    return v;
}

function frozenArrayLikeObjectReviver(k, v) {
    if (k === "a") {
        var obj = {};
        obj[0] = 'unmodifiable';
        obj.length = 1; 
        this.b = Object.freeze(obj);
        return v;
    }
    if (k === "0")
        return "modified";
    return v;
}

function frozenArrayReviverWithDelete(k, v) {
    if (k === "a") {
        this.b = Object.freeze(["unmodifiable"]);
        return v;
    }
    if (k === "0")
        return undefined;
    return v;
}

function frozenArrayLikeObjectReviverWithDelete(k, v) {
    if (k === "a") {
        var obj = {};
        obj[0] = 'unmodifiable';
        obj.length = 1; 
        this.b = Object.freeze(obj);
        return v;
    }
    if (k === "0")
        return undefined;
    return v;
}

function runTest(testId, reviver, expectedValue, expectedException) {
    let numIterations = 10000;
    for (var i = 0; i < numIterations; i++) {
        var exception = undefined;

        var obj;
        try {
            obj = JSON.parse('{ "a": 0, "b": 0 }', reviver);
        } catch (e) {
            exception = "" + e;
            exception = exception.substr(0, 10); // Search for "TypeError:".
        }
        shouldEqual(testId, exception, expectedException);
        shouldEqual(testId, obj.b[0], expectedValue);
    }
}

runTest(10000, frozenArrayReviver,                     "unmodifiable", undefined);
runTest(10001, frozenArrayLikeObjectReviver,           "unmodifiable", undefined);
runTest(10002, frozenArrayReviverWithDelete,           "unmodifiable", undefined);
runTest(10003, frozenArrayLikeObjectReviverWithDelete, "unmodifiable", undefined);
