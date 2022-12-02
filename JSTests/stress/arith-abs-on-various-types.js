//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "0"],
    ["1.", "1"],
    ["42", "42"],
    ["-42", "42"],
    ["Math.E", "Math.E"],
    ["Infinity", "Infinity"],
    ["-Infinity", "Infinity"],
    ["NaN", "NaN"],
    ["-NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "4"],
    ["\"-4\"", "4"],
    ["{ valueOf: () => { return Math.E; } }", "Math.E"],
    ["{ valueOf: () => { return 4; } }", "4"],
    ["{ valueOf: () => { return -4; } }", "4"],
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


// Test Math.abs() without arguments.
function opaqueAbsNoArgument() {
    return Math.abs();
}
noInline(opaqueAbsNoArgument);
noOSRExitFuzzing(opaqueAbsNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueAbsNoArgument();
        if (output === output) {
            throw "Failed opaqueAbsNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueAbsNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.abs() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesAbs(argument) {
    return Math.abs(argument);
}
noInline(opaqueAllTypesAbs);
noOSRExitFuzzing(opaqueAllTypesAbs);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesAbs(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesAbs) > 3)
        throw "We should have detected abs() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.abs() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueAbs(argument) {
                return Math.abs(argument);
            }
            noInline(opaqueAbs);
            noOSRExitFuzzing(opaqueAbs);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueAbs(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueAbs) > 1)
                throw "We should have compiled a single abs for the expected type. The argument was " + ${testCaseInput[0]};
        `);
    }
}
testSingleTypeCall();


// Test Math.abs() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueAbsOnConstant() {
                return Math.abs(${testCaseInput[0]});
            }
            noInline(opaqueAbsOnConstant);
            noOSRExitFuzzing(opaqueAbsOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueAbsOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueAbsOnConstant) > 1)
                throw "We should have compiled a single abs for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueAbsForSideEffects(argument) {
    return Math.abs(argument);
}
noInline(opaqueAbsForSideEffects);
noOSRExitFuzzing(opaqueAbsForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let abs16 = Math.abs(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueAbsForSideEffects(testObject) !== abs16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueAbsForSideEffects) > 1)
        throw "opaqueAbsForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify abs() is not subject to CSE if the argument has side effects.
function opaqueAbsForCSE(argument) {
    return Math.abs(argument) + Math.abs(argument) + Math.abs(argument);
}
noInline(opaqueAbsForCSE);
noOSRExitFuzzing(opaqueAbsForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let abs16 = Math.abs(16);
    let threeAbs16 = abs16 + abs16 + abs16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueAbsForCSE(testObject) !== threeAbs16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueAbsForCSE) > 1)
        throw "opaqueAbsForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify abs() is not subject to DCE if the argument has side effects.
function opaqueAbsForDCE(argument) {
    Math.abs(argument);
}
noInline(opaqueAbsForDCE);
noOSRExitFuzzing(opaqueAbsForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueAbsForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueAbsForDCE) > 1)
        throw "opaqueAbsForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueAbsWithException(argument) {
        let result = Math.abs(argument);
        ++counter;
        return result;
    }
    noInline(opaqueAbsWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let abs64 = Math.abs(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueAbsWithException(testObject) !== abs64)
            throw "Incorrect result in opaqueAbsWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueAbsWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueAbsWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
