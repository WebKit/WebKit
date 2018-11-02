// Test that throw an OOM exception when compiling a pathological, but valid nested RegExp.

function recurseAndTest(depth, f, expectedException)
{
    // Probe stack depth
    try {
        let result = recurseAndTest(depth + 1, f, expectedException);
        if (result == 0) {
            try {
                // Call the test function with a nearly full stack.
                f();
            } catch (e) {
                return e.toString();
            }

            return 1;
        } else if (result < 0)
            return result + 1;
        else
            return result;
    } catch (e) {
        // Go up a several frames and then call the test function
        return -10;
    }

    return 1;
}

let deepRE = /((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((x))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))/;
let matchLen = 381; // The number of parens plus 1 for the whole match.

let regExpOOMError = "Error: Out of memory: Invalid regular expression: too many nested disjunctions";

// Test that both exec (captured compilation) and test (match only compilation) handles OOM.
let result = recurseAndTest(1, () => { deepRE.exec(); });
if (result != regExpOOMError)
    throw "Expected: \"" + regExpOOMError + "\" but got \"" + result + "\"";

result = recurseAndTest(1, () => { deepRE.test(); });
if (result != regExpOOMError)
    throw "Expected: \"" + regExpOOMError + "\" but got \"" + result + "\"";

// Test that the RegExp works correctly with RegExp.exec() and RegExp.test() when there is sufficient stack space to compile it.
let m = deepRE.exec("x");
let matched = true;
if (m.length != matchLen)
    matched = false
else {
    for (i = 0; i < matchLen; i++) {
        if (m[i] != "x")
            matched = false;
    }
}

if (!matched) {
    let expectedMatch = [];
    for (i = 0; i < matchLen; i++)
        expectedMatch[i] = "x";

    throw "Expected RegExp.exec(...) to be [" + expectedMatch + "] but got [" + m + "]";
}

if (!deepRE.test("x"))
    throw "Expected RegExp.test(...) to be true, but was false";
