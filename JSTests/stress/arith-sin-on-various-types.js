//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let sinOfFour = Math.sin(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["4", "" + sinOfFour],
    ["Math.PI", "" + Math.sin(Math.PI)],
    ["Infinity", "NaN"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + sinOfFour],
    ["{ valueOf: () => { return 4; } }", "" + sinOfFour],
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


// Test Math.sin() without arguments.
function opaqueSinNoArgument() {
    return Math.sin();
}
noInline(opaqueSinNoArgument);
noOSRExitFuzzing(opaqueSinNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueSinNoArgument();
        if (output === output) {
            throw "Failed opaqueSinNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueSinNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.sin() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesSin(argument) {
    return Math.sin(argument);
}
noInline(opaqueAllTypesSin);
noOSRExitFuzzing(opaqueAllTypesSin);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesSin(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesSin) > 2)
        throw "We should have detected sin() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.sin() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueSin(argument) {
                return Math.sin(argument);
            }
            noInline(opaqueSin);
            noOSRExitFuzzing(opaqueSin);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueSin(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueSin) > 1)
                throw "We should have compiled a single sin for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.sin() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueSinOnConstant() {
                return Math.sin(${testCaseInput[0]});
            }
            noInline(opaqueSinOnConstant);
            noOSRExitFuzzing(opaqueSinOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueSinOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueSinOnConstant) > 1)
                throw "We should have compiled a single sin for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueSinForSideEffects(argument) {
    return Math.sin(argument);
}
noInline(opaqueSinForSideEffects);
noOSRExitFuzzing(opaqueSinForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let sin16 = Math.sin(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueSinForSideEffects(testObject) !== sin16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueSinForSideEffects) > 1)
        throw "opaqueSinForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify sin() is not subject to CSE if the argument has side effects.
function opaqueSinForCSE(argument) {
    return Math.sin(argument) + Math.sin(argument) + Math.sin(argument);
}
noInline(opaqueSinForCSE);
noOSRExitFuzzing(opaqueSinForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let sin16 = Math.sin(16);
    let threeSin16 = sin16 + sin16 + sin16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueSinForCSE(testObject) !== threeSin16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueSinForCSE) > 1)
        throw "opaqueSinForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify sin() is not subject to DCE if the argument has side effects.
function opaqueSinForDCE(argument) {
    Math.sin(argument);
}
noInline(opaqueSinForDCE);
noOSRExitFuzzing(opaqueSinForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueSinForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueSinForDCE) > 1)
        throw "opaqueSinForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueSinWithException(argument) {
        let result = Math.sin(argument);
        ++counter;
        return result;
    }
    noInline(opaqueSinWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let sin64 = Math.sin(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueSinWithException(testObject) !== sin64)
            throw "Incorrect result in opaqueSinWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueSinWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueSinWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
