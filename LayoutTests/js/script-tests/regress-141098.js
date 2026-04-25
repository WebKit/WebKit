//@ skip

description("Regression test for https://webkit.org/b/141098. Make sure eval() properly handles running out of stack space. This test should run without crashing.");

// The tiering up to test higher levels of optimization will only test the DFG
// if run in run-jsc-stress-tests with the eager settings.

// countStart, countIncrement, and numberOfFramesToBackoffFromStackOverflowPoint were
// empirically determined to be the values that will cause StackOverflowErrors to be
// thrown where the tests expects them. If stack frame layouts change sufficiently (e.g.
// due to JIT changes) such that this test starts failing (due to text output
// differences), then these values will need to be rebased.
// Under no circumstance should this test ever crash.
let countStart = 2;
let countIncrement = 8;
let numberOfFramesToBackoffFromStackOverflowPoint = 10;

// backoffEverything is chosen to be -1 because a negative number will never be
// decremented to 0, and hence, will cause probeAndRecurse() to return out of every
// frame instead of retrying the eval test.
let backoffEverything = -1;

var lastEvalString = "";

function testEval(maxIterations)
{
    var result;
    var count = countStart;

    if (!maxIterations)
        var result = eval(lastEvalString);
    else {
        for (var iter = 0; iter < maxIterations; count *= countIncrement, iter++) {
            var evalString = "\"dummy\".valueOf(";

            for (var i = 0; i < count; i++) {
                if (i > 0)
                    evalString += ", ";
                evalString += i;
            }

            evalString +=  ");";

            if (maxIterations > 1)
                lastEvalString = evalString;
            result = eval(evalString);
        }
    }

    return result;
}

function probeAndRecurse(reuseEvalString)
{
    var framesToBackOffFromStackOverflowPoint;

    // Probe stack depth
    try {
        remainingFramesToBackOff = probeAndRecurse(reuseEvalString);

        if (!remainingFramesToBackOff) {
            // We've backed off a number of frames. Now retry the eval test to see if we
            // still overflow.
            try {
                testEval(1);
            } catch (e) {
                // Yikes. We still overflow. Back off some more.
                return numberOfFramesToBackoffFromStackOverflowPoint;
            }
        } else
            return remainingFramesToBackOff - 1
    } catch (e) {
        // We exceeded stack space, now return up the stack until we can execute a simple eval.
        // Then run an eval test to exceed stack.
        return numberOfFramesToBackoffFromStackOverflowPoint;
    }

    try {
        testEval(reuseEvalString ? 0 : 20);
    } catch (e) {
        testPassed("Exception: " + e);
    }

    return backoffEverything;
}

// Because this test intentionally exhausts the stack, we call testPassed() to ensure
// everything we need in that path has been compiled and is available.  Otherwise we
// might properly handle an out of stack, but run out of stack calling testPassed().
testPassed("Initial setup");

debug("Starting 1st probeAndRecurse");
probeAndRecurse(false);

// Tier up the eval'ed code.
// When run with run-jsc-stress-tests and it's agressive options, this low of a count will
// allow us to get up to the DFG.
for (var i = 0; i < 200; i++)
    testEval(0);

debug("Starting 2nd probeAndRecurse");
probeAndRecurse(true);
