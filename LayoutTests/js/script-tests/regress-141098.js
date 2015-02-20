description("Regression test for https://webkit.org/b/141098. Make sure eval() properly handles running out of stack space. This test should run without crashing.");

// The tiering up to test higher levels of optimization will only test the DFG
// if run in run-jsc-stress-tests with the eager settings.

var lastEvalString = "";

function testEval(maxIterations)
{
    var result;
    var count = 1;

    if (!maxIterations) {
        var result = eval(lastEvalString);
    } else {
        for (var iter = 0; iter < maxIterations; count *= 4, iter++) {
            var evalString = "\"dummy\".valueOf(";

            for (var i = 0; i < count; i++) {
                if (i > 0)
                    evalString += ", ";
                evalString += i;
            }

            evalString +=  ");";

            lastEvalString = evalString;
            result = eval(evalString);
        }
    }

    return result;
}

function probeAndRecurse(depth)
{
    var result;

    // Probe stack depth
    if (depth > 0) {
        try {
            result = probeAndRecurse(depth+1);

            if (!result) {
                try {
                    testEval(1);
                } catch (e) {
                    return -49;
                }
            } else
                return result + 1
        } catch (e) {
            // We exceeded stack space, now return up the stack until we can execute a simple eval.
            // Then run an eval test to exceed stack.
            return -49;
        }
    } else if (depth != 0)
        return probeAndRecurse(depth+1);

    try {
        testEval((depth > 0) ? 20 : 0);
    } catch (e) {
        testPassed("Exception: " + e);
    }

    return 1;
}

var depth = probeAndRecurse(1);

// Tier up the eval'ed code.
// When run with run-jsc-stress-tests and it's agressive options, this low of a count will
// allow us to get up to the DFG.
for (var i = 0; i < 200; i++)
    testEval(0);

probeAndRecurse(-depth);
