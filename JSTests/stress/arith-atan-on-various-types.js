//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let atanOfFour = Math.atan(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["4", "" + atanOfFour],
    ["Math.PI", "" + Math.atan(Math.PI)],
    ["Infinity", "" + Math.atan(Infinity)],
    ["-Infinity", "-" + Math.atan(Infinity)],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + atanOfFour],
    ["{ valueOf: () => { return 4; } }", "" + atanOfFour],
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


// Test Math.atan() without arguments.
function opaqueATanNoArgument() {
    return Math.atan();
}
noInline(opaqueATanNoArgument);
noOSRExitFuzzing(opaqueATanNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueATanNoArgument();
        if (output === output) {
            throw "Failed opaqueATanNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueATanNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.atan() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesATan(argument) {
    return Math.atan(argument);
}
noInline(opaqueAllTypesATan);
noOSRExitFuzzing(opaqueAllTypesATan);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesATan(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesATan) > 2)
        throw "We should have detected atan() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.atan() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueATan(argument) {
                return Math.atan(argument);
            }
            noInline(opaqueATan);
            noOSRExitFuzzing(opaqueATan);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueATan(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueATan) > 1)
                throw "We should have compiled a single atan for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.atan() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueATanOnConstant() {
                return Math.atan(${testCaseInput[0]});
            }
            noInline(opaqueATanOnConstant);
            noOSRExitFuzzing(opaqueATanOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueATanOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueATanOnConstant) > 1)
                throw "We should have compiled a single atan for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueATanForSideEffects(argument) {
    return Math.atan(argument);
}
noInline(opaqueATanForSideEffects);
noOSRExitFuzzing(opaqueATanForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let AtanResult = Math.atan(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueATanForSideEffects(testObject) !== AtanResult)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueATanForSideEffects) > 1)
        throw "opaqueATanForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify atan() is not subject to CSE if the argument has side effects.
function opaqueATanForCSE(argument) {
    return Math.atan(argument) + Math.atan(argument) + Math.atan(argument);
}
noInline(opaqueATanForCSE);
noOSRExitFuzzing(opaqueATanForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let AtanResult = Math.atan(16);
    let threeAtanResult = AtanResult + AtanResult + AtanResult;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueATanForCSE(testObject) !== threeAtanResult)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueATanForCSE) > 1)
        throw "opaqueATanForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify atan() is not subject to DCE if the argument has side effects.
function opaqueATanForDCE(argument) {
    Math.atan(argument);
}
noInline(opaqueATanForDCE);
noOSRExitFuzzing(opaqueATanForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueATanForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueATanForDCE) > 1)
        throw "opaqueATanForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueATanWithException(argument) {
        let result = Math.atan(argument);
        ++counter;
        return result;
    }
    noInline(opaqueATanWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let atanResult = Math.atan(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueATanWithException(testObject) !== atanResult)
            throw "Incorrect result in opaqueATanWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueATanWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueATanWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
