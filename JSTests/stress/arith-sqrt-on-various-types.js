"use strict";

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0."],
    ["4", "2"],
    ["42.5", "6.519202405202649"],
    ["Infinity", "Infinity"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "2"],
    ["{ valueOf: () => { return 4; } }", "2"],
];

let validInputTypedTestCases = validInputTestCases.map((element) => { return [eval("(" + element[0] + ")"), eval(element[1])] });

function isIdentical(result, expected)
{
    if (expected === expected) {
        if (result !== expected)
            return false;
        if (!expected && 1 / expected === -Infinity && 1 / result !== -Infinity)
            return false;

        return true;
    }
    return result !== result;
}


// Test Math.sqrt() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesSqrt(argument) {
    return Math.sqrt(argument);
}
noInline(opaqueAllTypesSqrt);
noOSRExitFuzzing(opaqueAllTypesSqrt);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesSqrt(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesSqrt) > 2)
        throw "We should have detected sqrt() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.sqrt() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueSqrt(argument) {
                return Math.sqrt(argument);
            }
            noInline(opaqueSqrt);
            noOSRExitFuzzing(opaqueSqrt);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueSqrt(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueSqrt) > 1)
                throw "We should have compiled a single sqrt for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Verify we call valueOf() exactly once per call.
function opaqueSqrtForSideEffects(argument) {
    return Math.sqrt(argument);
}
noInline(opaqueSqrtForSideEffects);
noOSRExitFuzzing(opaqueSqrtForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueSqrtForSideEffects(testObject) !== 4)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueSqrtForSideEffects) > 1)
        throw "opaqueSqrtForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify SQRT is not subject to CSE if the argument has side effects.
function opaqueSqrtForCSE(argument) {
    return Math.sqrt(argument) + Math.sqrt(argument) + Math.sqrt(argument);
}
noInline(opaqueSqrtForCSE);
noOSRExitFuzzing(opaqueSqrtForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueSqrtForCSE(testObject) !== 12)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueSqrtForCSE) > 1)
        throw "opaqueSqrtForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueSqrtWithException(argument) {
        let result = Math.sqrt(argument);
        ++counter;
        return result;
    }
    noInline(opaqueSqrtWithException);

    let testObject = { valueOf: () => {  return 64; } };

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueSqrtWithException(testObject) !== 8)
            throw "Incorrect result in opaqueSqrtWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueSqrtWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueSqrtWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
