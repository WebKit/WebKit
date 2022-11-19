//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let cbrtOfHalf = Math.cbrt(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["1", "1"],
    ["0", "0"],
    ["-0.", "-0"],
    ["-42.", "" + Math.cbrt(-42)],
    ["0.5", "" + cbrtOfHalf],
    ["Math.PI", "" + Math.cbrt(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "-Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + cbrtOfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + cbrtOfHalf],
];

let validInputTypedTestCases = validInputTestCases.map((element) => { return [eval("(" + element[0] + ")"), eval(element[1])] });

function isIdentical(result, expected)
{
    if (expected === expected) {
        if (result !== expected)
            return false;
        if (!expected)
            return (1 / expected) === (1 / result);

        return true;
    }
    return result !== result;
}


// Test Math.cbrt() without arguments.
function opaqueCbrtNoArgument() {
    return Math.cbrt();
}
noInline(opaqueCbrtNoArgument);
noOSRExitFuzzing(opaqueCbrtNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueCbrtNoArgument();
        if (output === output) {
            throw "Failed opaqueCbrtNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueCbrtNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.cbrt() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesCbrt(argument) {
    return Math.cbrt(argument);
}
noInline(opaqueAllTypesCbrt);
noOSRExitFuzzing(opaqueAllTypesCbrt);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesCbrt(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesCbrt) > 2)
        throw "We should have detected cbrt() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.cbrt() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCbrt(argument) {
                return Math.cbrt(argument);
            }
            noInline(opaqueCbrt);
            noOSRExitFuzzing(opaqueCbrt);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCbrt(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueCbrt) > 1)
                throw "We should have compiled a single cbrt for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.cbrt() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCbrtOnConstant() {
                return Math.cbrt(${testCaseInput[0]});
            }
            noInline(opaqueCbrtOnConstant);
            noOSRExitFuzzing(opaqueCbrtOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCbrtOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueCbrtOnConstant) > 1)
                throw "We should have compiled a single cbrt for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueCbrtForSideEffects(argument) {
    return Math.cbrt(argument);
}
noInline(opaqueCbrtForSideEffects);
noOSRExitFuzzing(opaqueCbrtForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let cbrtResult = Math.cbrt(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCbrtForSideEffects(testObject) !== cbrtResult)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueCbrtForSideEffects) > 1)
        throw "opaqueCbrtForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify cbrt() is not subject to CSE if the argument has side effects.
function opaqueCbrtForCSE(argument) {
    return Math.cbrt(argument) + Math.cbrt(argument) + Math.cbrt(argument);
}
noInline(opaqueCbrtForCSE);
noOSRExitFuzzing(opaqueCbrtForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let cbrtResult = Math.cbrt(0.2);
    let threecbrtResult = cbrtResult + cbrtResult + cbrtResult;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCbrtForCSE(testObject) !== threecbrtResult)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueCbrtForCSE) > 1)
        throw "opaqueCbrtForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify cbrt() is not subject to DCE if the argument has side effects.
function opaqueCbrtForDCE(argument) {
    Math.cbrt(argument);
}
noInline(opaqueCbrtForDCE);
noOSRExitFuzzing(opaqueCbrtForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueCbrtForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueCbrtForDCE) > 1)
        throw "opaqueCbrtForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueCbrtWithException(argument) {
        let result = Math.cbrt(argument);
        ++counter;
        return result;
    }
    noInline(opaqueCbrtWithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let cbrtResult = Math.cbrt(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueCbrtWithException(testObject) !== cbrtResult)
            throw "Incorrect result in opaqueCbrtWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueCbrtWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueCbrtWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
