//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let acoshOfFour = Math.acosh(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "NaN"],
    ["0", "NaN"],
    ["-0.", "NaN"],
    ["4", "" + acoshOfFour],
    ["Math.PI", "" + Math.acosh(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + acoshOfFour],
    ["{ valueOf: () => { return 4; } }", "" + acoshOfFour],
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


// Test Math.acosh() without arguments.
function opaqueACoshNoArgument() {
    return Math.acosh();
}
noInline(opaqueACoshNoArgument);
noOSRExitFuzzing(opaqueACoshNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueACoshNoArgument();
        if (output === output) {
            throw "Failed opaqueACoshNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueACoshNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.acosh() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesACosh(argument) {
    return Math.acosh(argument);
}
noInline(opaqueAllTypesACosh);
noOSRExitFuzzing(opaqueAllTypesACosh);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesACosh(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesACosh) > 2)
        throw "We should have detected acosh() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.acosh() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueACosh(argument) {
                return Math.acosh(argument);
            }
            noInline(opaqueACosh);
            noOSRExitFuzzing(opaqueACosh);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueACosh(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueACosh) > 1)
                throw "We should have compiled a single acosh for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.acosh() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueACoshOnConstant() {
                return Math.acosh(${testCaseInput[0]});
            }
            noInline(opaqueACoshOnConstant);
            noOSRExitFuzzing(opaqueACoshOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueACoshOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueACoshOnConstant) > 1)
                throw "We should have compiled a single acosh for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueACoshForSideEffects(argument) {
    return Math.acosh(argument);
}
noInline(opaqueACoshForSideEffects);
noOSRExitFuzzing(opaqueACoshForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let acosh16 = Math.acosh(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueACoshForSideEffects(testObject) !== acosh16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueACoshForSideEffects) > 1)
        throw "opaqueACoshForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify acosh() is not subject to CSE if the argument has side effects.
function opaqueACoshForCSE(argument) {
    return Math.acosh(argument) + Math.acosh(argument) + Math.acosh(argument);
}
noInline(opaqueACoshForCSE);
noOSRExitFuzzing(opaqueACoshForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let acosh16 = Math.acosh(16);
    let threeACosh16 = acosh16 + acosh16 + acosh16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueACoshForCSE(testObject) !== threeACosh16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueACoshForCSE) > 1)
        throw "opaqueACoshForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify acosh() is not subject to DCE if the argument has side effects.
function opaqueACoshForDCE(argument) {
    Math.acosh(argument);
}
noInline(opaqueACoshForDCE);
noOSRExitFuzzing(opaqueACoshForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueACoshForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueACoshForDCE) > 1)
        throw "opaqueACoshForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueACoshWithException(argument) {
        let result = Math.acosh(argument);
        ++counter;
        return result;
    }
    noInline(opaqueACoshWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let acosh64 = Math.acosh(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueACoshWithException(testObject) !== acosh64)
            throw "Incorrect result in opaqueACoshWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueACoshWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueACoshWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
