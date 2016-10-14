//@ runFTLNoCJIT

let totalFailed = 0;

function shouldEqual(testId, actual, expected) {
    if (actual != expected) {
        throw testId + ": ERROR: expect " + expected + ", actual " + actual;
    }
}

function makeArray() {
    return ['unmodifiable'];
}

function makeArrayLikeObject() {
    var obj = {};
    obj[0] = 'unmodifiable';
    obj.length = 1; 
    return obj;
}

function emptyArraySourceMaker() {
    return [];
}

function singleElementArraySourceMaker() {
    return ['modified_1'];
}

// Make test functions with unique codeblocks.
function makeSpliceWithNoArgsTest(testId) {
    return new Function("arr", "arr.splice(); return " + testId + ";");
}
function makeSpliceTest(testId) {
    return new Function("arr", "arr.splice(0); return " + testId + ";");
}

let numIterations = 10000;

function runTest(testId, testMaker, targetMaker, sourceMaker, expectedValue, expectedException) {
    var test = testMaker(testId);
    noInline(test);

    for (var i = 0; i < numIterations; i++) {
        var exception = undefined;

        var obj = targetMaker();
        obj = Object.freeze(obj);

        var arr = sourceMaker();
        arr.constructor = { [Symbol.species]: function() { return obj; } };

        try {
            test(arr);
        } catch (e) {
            exception = "" + e;
            exception = exception.substr(0, 10); // Search for "TypeError:".
        }
        shouldEqual(testId, exception, expectedException);
        shouldEqual(testId, obj[0], expectedValue);
    }
}

runTest(10010, makeSpliceWithNoArgsTest, makeArray,           emptyArraySourceMaker,         "unmodifiable", "TypeError:");
runTest(10011, makeSpliceWithNoArgsTest, makeArray,           singleElementArraySourceMaker, "unmodifiable", "TypeError:");
runTest(10020, makeSpliceWithNoArgsTest, makeArrayLikeObject, emptyArraySourceMaker,         "unmodifiable", "TypeError:");
runTest(10021, makeSpliceWithNoArgsTest, makeArrayLikeObject, singleElementArraySourceMaker, "unmodifiable", "TypeError:");

runTest(10110, makeSpliceTest, makeArray,           emptyArraySourceMaker,         "unmodifiable", "TypeError:");
runTest(10111, makeSpliceTest, makeArray,           singleElementArraySourceMaker, "unmodifiable", "TypeError:");
runTest(10120, makeSpliceTest, makeArrayLikeObject, emptyArraySourceMaker,         "unmodifiable", "TypeError:");
runTest(10121, makeSpliceTest, makeArrayLikeObject, singleElementArraySourceMaker, "unmodifiable", "TypeError:");
