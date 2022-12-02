//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let coshOfFour = Math.cosh(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "1"],
    ["0", "1"],
    ["-0.", "1"],
    ["4", "" + coshOfFour],
    ["Math.PI", "" + Math.cosh(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + coshOfFour],
    ["{ valueOf: () => { return 4; } }", "" + coshOfFour],
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


// Test Math.cosh() without arguments.
function opaqueCoshNoArgument() {
    return Math.cosh();
}
noInline(opaqueCoshNoArgument);
noOSRExitFuzzing(opaqueCoshNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueCoshNoArgument();
        if (output === output) {
            throw "Failed opaqueCoshNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueCoshNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.cosh() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesCosh(argument) {
    return Math.cosh(argument);
}
noInline(opaqueAllTypesCosh);
noOSRExitFuzzing(opaqueAllTypesCosh);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesCosh(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesCosh) > 2)
        throw "We should have detected cosh() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.cosh() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCosh(argument) {
                return Math.cosh(argument);
            }
            noInline(opaqueCosh);
            noOSRExitFuzzing(opaqueCosh);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCosh(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueCosh) > 1)
                throw "We should have compiled a single cosh for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.cosh() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCoshOnConstant() {
                return Math.cosh(${testCaseInput[0]});
            }
            noInline(opaqueCoshOnConstant);
            noOSRExitFuzzing(opaqueCoshOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCoshOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueCoshOnConstant) > 1)
                throw "We should have compiled a single cosh for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueCoshForSideEffects(argument) {
    return Math.cosh(argument);
}
noInline(opaqueCoshForSideEffects);
noOSRExitFuzzing(opaqueCoshForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let cosh16 = Math.cosh(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCoshForSideEffects(testObject) !== cosh16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueCoshForSideEffects) > 1)
        throw "opaqueCoshForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify cosh() is not subject to CSE if the argument has side effects.
function opaqueCoshForCSE(argument) {
    return Math.cosh(argument) + Math.cosh(argument) + Math.cosh(argument);
}
noInline(opaqueCoshForCSE);
noOSRExitFuzzing(opaqueCoshForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let cosh16 = Math.cosh(16);
    let threeCosh16 = cosh16 + cosh16 + cosh16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCoshForCSE(testObject) !== threeCosh16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueCoshForCSE) > 1)
        throw "opaqueCoshForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify cosh() is not subject to DCE if the argument has side effects.
function opaqueCoshForDCE(argument) {
    Math.cosh(argument);
}
noInline(opaqueCoshForDCE);
noOSRExitFuzzing(opaqueCoshForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueCoshForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueCoshForDCE) > 1)
        throw "opaqueCoshForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueCoshWithException(argument) {
        let result = Math.cosh(argument);
        ++counter;
        return result;
    }
    noInline(opaqueCoshWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let cosh64 = Math.cosh(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueCoshWithException(testObject) !== cosh64)
            throw "Incorrect result in opaqueCoshWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueCoshWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueCoshWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
