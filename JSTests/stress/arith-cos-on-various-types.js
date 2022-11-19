//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let cosOfFour = Math.cos(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "1"],
    ["0", "1"],
    ["-0.", "1"],
    ["4", "" + cosOfFour],
    ["Math.PI", "-1"],
    ["Infinity", "NaN"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + cosOfFour],
    ["{ valueOf: () => { return 4; } }", "" + cosOfFour],
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


// Test Math.cos() without arguments.
function opaqueCosNoArgument() {
    return Math.cos();
}
noInline(opaqueCosNoArgument);
noOSRExitFuzzing(opaqueCosNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueCosNoArgument();
        if (output === output) {
            throw "Failed opaqueCosNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueCosNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.cos() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesCos(argument) {
    return Math.cos(argument);
}
noInline(opaqueAllTypesCos);
noOSRExitFuzzing(opaqueAllTypesCos);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesCos(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesCos) > 2)
        throw "We should have detected cos() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.cos() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCos(argument) {
                return Math.cos(argument);
            }
            noInline(opaqueCos);
            noOSRExitFuzzing(opaqueCos);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCos(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueCos) > 1)
                throw "We should have compiled a single cos for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.cos() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCosOnConstant() {
                return Math.cos(${testCaseInput[0]});
            }
            noInline(opaqueCosOnConstant);
            noOSRExitFuzzing(opaqueCosOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCosOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueCosOnConstant) > 1)
                throw "We should have compiled a single cos for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueCosForSideEffects(argument) {
    return Math.cos(argument);
}
noInline(opaqueCosForSideEffects);
noOSRExitFuzzing(opaqueCosForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let cos16 = Math.cos(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCosForSideEffects(testObject) !== cos16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueCosForSideEffects) > 1)
        throw "opaqueCosForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify cos() is not subject to CSE if the argument has side effects.
function opaqueCosForCSE(argument) {
    return Math.cos(argument) + Math.cos(argument) + Math.cos(argument);
}
noInline(opaqueCosForCSE);
noOSRExitFuzzing(opaqueCosForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let cos16 = Math.cos(16);
    let threeCos16 = cos16 + cos16 + cos16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCosForCSE(testObject) !== threeCos16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueCosForCSE) > 1)
        throw "opaqueCosForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify cos() is not subject to DCE if the argument has side effects.
function opaqueCosForDCE(argument) {
    Math.cos(argument);
}
noInline(opaqueCosForDCE);
noOSRExitFuzzing(opaqueCosForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueCosForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueCosForDCE) > 1)
        throw "opaqueCosForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueCosWithException(argument) {
        let result = Math.cos(argument);
        ++counter;
        return result;
    }
    noInline(opaqueCosWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let cos64 = Math.cos(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueCosWithException(testObject) !== cos64)
            throw "Incorrect result in opaqueCosWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueCosWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueCosWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
