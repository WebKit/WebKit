//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let tanOfFour = Math.tan(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["4", "" + tanOfFour],
    ["Math.PI", "" + Math.tan(Math.PI)],
    ["Infinity", "NaN"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + tanOfFour],
    ["{ valueOf: () => { return 4; } }", "" + tanOfFour],
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


// Test Math.tan() without arguments.
function opaqueTanNoArgument() {
    return Math.tan();
}
noInline(opaqueTanNoArgument);
noOSRExitFuzzing(opaqueTanNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueTanNoArgument();
        if (output === output) {
            throw "Failed opaqueTanNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueTanNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.tan() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesTan(argument) {
    return Math.tan(argument);
}
noInline(opaqueAllTypesTan);
noOSRExitFuzzing(opaqueAllTypesTan);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesTan(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesTan) > 2)
        throw "We should have detected tan() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.tan() on a completely typed input. Every call see only one type.
function testTangleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTan(argument) {
                return Math.tan(argument);
            }
            noInline(opaqueTan);
            noOSRExitFuzzing(opaqueTan);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTan(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testTangleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueTan) > 1)
                throw "We should have compiled a tangle tan for the expected type.";
        `);
    }
}
testTangleTypeCall();


// Test Math.tan() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTanOnConstant() {
                return Math.tan(${testCaseInput[0]});
            }
            noInline(opaqueTanOnConstant);
            noOSRExitFuzzing(opaqueTanOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTanOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueTanOnConstant) > 1)
                throw "We should have compiled a tangle tan for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueTanForSideEffects(argument) {
    return Math.tan(argument);
}
noInline(opaqueTanForSideEffects);
noOSRExitFuzzing(opaqueTanForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let tan16 = Math.tan(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueTanForSideEffects(testObject) !== tan16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueTanForSideEffects) > 1)
        throw "opaqueTanForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify tan() is not subject to CSE if the argument has side effects.
function opaqueTanForCSE(argument) {
    return Math.tan(argument) + Math.tan(argument) + Math.tan(argument);
}
noInline(opaqueTanForCSE);
noOSRExitFuzzing(opaqueTanForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let tan16 = Math.tan(16);
    let threeTan16 = tan16 + tan16 + tan16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueTanForCSE(testObject) !== threeTan16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueTanForCSE) > 1)
        throw "opaqueTanForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify tan() is not subject to DCE if the argument has side effects.
function opaqueTanForDCE(argument) {
    Math.tan(argument);
}
noInline(opaqueTanForDCE);
noOSRExitFuzzing(opaqueTanForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueTanForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueTanForDCE) > 1)
        throw "opaqueTanForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueTanWithException(argument) {
        let result = Math.tan(argument);
        ++counter;
        return result;
    }
    noInline(opaqueTanWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let tan64 = Math.tan(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueTanWithException(testObject) !== tan64)
            throw "Incorrect result in opaqueTanWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueTanWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueTanWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
