// This test module provides infrastructure for generating and running tests on a binary
// operator.
//
// It works by generating test functions to exercise the specified operator using
// from permutations of operand value pairs. For each test, it computes the expected
// result by exercising the test function once (using the LLINT) at test generation time.
// The test runner later compares the result produced by the function (as it tiers up)
// against the expected result.
//
// The generated tests can exercise the operator using the following operand types.
//     variable * variable
//     constant * variable
//     variable * constant
//
// If all goes well, this test module will terminate silently. If not, it will print
// errors.

//============================================================================
// Debugging options:

var verbose = false;
var abortOnFirstFail = false;
var testFilterStr = undefined; // Define a filter string to filter tests to run.

var verboseTestGeneration = false;

//============================================================================
// Test generation:

function stringifyIfNeeded(x) {
    if (typeof x == "string")
        return '"' + x + '"';
    return x;
}

// operandTypes are "VarVar", "VarConst", and "ConstVar".
var funcIndex = 0;
function generateBinaryTests(tests, opName, op, operandTypes, leftValues, rightValues) {
    var funcNamePrefix = opName + operandTypes;
    for (var i = 0; i < leftValues.length; i++) {
        for (var j = 0; j < rightValues.length; j++) {
            var test = { };
            xStr = leftValues[i];
            yStr = rightValues[j];
            test.x = eval(xStr);
            test.y = eval(yStr);

            var funcName = funcNamePrefix + funcIndex++;
            if (operandTypes == "VarVar") {
                test.signature = funcName + "(x, y) { return x " + op + " y }";
                test.name = test.signature + " with x:" + xStr + ", y:" + yStr;
            } else if (operandTypes == "VarConst") {
                test.signature = funcName + "(x, _) { return x " + op + " " + yStr + " }";
                test.name = test.signature + " with x:" + xStr;
            } else if (operandTypes == "ConstVar") {
                test.signature = funcName + "(_, y) { return " + xStr + " " + op + " y }";
                test.name = test.signature + " with y:" + yStr;
            }

            test.func = eval("(function " + test.signature + ")");
            noInline(test.func);

            test.expectedResult = test.func(test.x, test.y);
            test.name += ", expected:" + stringifyIfNeeded(test.expectedResult);

            tests.push(test);
            if (verboseTestGeneration)
                print("Generated " + test.name);
        }
    }
}

//============================================================================
// Test running and reporting:

var errorReport = "";

function isIdentical(x, y) {
    if (typeof x != typeof y)
        return false;
    if (x == y) {
        if (x)
            return true;
        // Distinguish between 0 and negative 0.
        if (1 / x == 1 / y)
            return true;
    } else if (Number.isNaN(x) && Number.isNaN(y))
        return true;
    return false;
}

function runTest(test) {
    if (testFilterStr && !test.name.includes(testFilterStr))
        return;

    var firstFailed = -1;
    try {
        if (verbose)
            print(test.name);
        for (var i = 0; i < 10000; i++) {
            var result = test.func(test.x, test.y);
            if (isIdentical(result, test.expectedResult))
                continue;
            if (firstFailed < 0) {
                errorReport += "FAILED: " + test.name + " started failing on iteration " + i
                    + ": actual " + stringifyIfNeeded(result) + "\n";
                if (abortOnFirstFail)
                    throw errorReport;
                firstFailed = i;
            }
        }
    } catch(e) {
        if (abortOnFirstFail)
            throw e; // Negate the catch by re-throwing.
        errorReport += "FAILED: Unexpected exception: " + e + "\n";
    }
}

function run() {
    if (verbose)
        print("Start testing");

    for (var test of tests)
        runTest(test);
    // for (var i = 0; i < tests.length; i++)
    //     runTest(tests[i]);

    if (errorReport !== "")
        throw "Found failures:\n" + errorReport;

    if (verbose)
        print("Done testing");
}
