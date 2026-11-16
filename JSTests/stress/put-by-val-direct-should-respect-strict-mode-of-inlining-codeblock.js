//@ runDefault("--useRandomizingFuzzerAgent=1", "--useConcurrentJIT=0")

let totalFailed = 0;

function shouldEqual(testId, iteration, actual, expected) {
    if (actual != expected) {
        throw new Error("Test #" + testId + ", iteration " + iteration + ", ERROR: expected \"" + expected + "\", got \"" + actual + "\"");
    }
}

function makeUnwriteableUnconfigurableObject()
{
    return Object.defineProperty([], 0, {value: "frozen", writable: false, configurable: false});
}

function testArrayOf(obj)
{
    Array.of.call(function() { return obj; }, "no longer frozen");
}

noInline(testArrayOf);

let numIterations = 10000;

function runTest(testId, test, sourceMaker, expectedException) {
    for (var i = 0; i < numIterations; i++) {
        var exception = "No exception";
        var obj = sourceMaker();

        try {
            test(obj);
        } catch (e) {
            exception = "" + e;
            exception = exception.substr(0, 10); // Search for "TypeError:".
        }
        shouldEqual(testId, i, exception, expectedException);
    }
}

runTest(1, testArrayOf, makeUnwriteableUnconfigurableObject, "TypeError:");
