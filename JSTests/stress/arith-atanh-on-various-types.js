//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let atanhOfHalf = Math.atanh(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["1", "Infinity"],
    ["0", "0"],
    ["-0.", "-0"],
    ["0.5", "" + atanhOfHalf],
    ["Math.PI", "" + Math.atanh(Math.PI)],
    ["Infinity", "NaN"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + atanhOfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + atanhOfHalf],
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


// Test Math.atanh() without arguments.
function opaqueATanhNoArgument() {
    return Math.atanh();
}
noInline(opaqueATanhNoArgument);
noOSRExitFuzzing(opaqueATanhNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueATanhNoArgument();
        if (output === output) {
            throw "Failed opaqueATanhNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueATanhNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.atanh() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesATanh(argument) {
    return Math.atanh(argument);
}
noInline(opaqueAllTypesATanh);
noOSRExitFuzzing(opaqueAllTypesATanh);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesATanh(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesATanh) > 2)
        throw "We should have detected atanh() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.atanh() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueATanh(argument) {
                return Math.atanh(argument);
            }
            noInline(opaqueATanh);
            noOSRExitFuzzing(opaqueATanh);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueATanh(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueATanh) > 1)
                throw "We should have compiled a single atanh for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.atanh() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueATanhOnConstant() {
                return Math.atanh(${testCaseInput[0]});
            }
            noInline(opaqueATanhOnConstant);
            noOSRExitFuzzing(opaqueATanhOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueATanhOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueATanhOnConstant) > 1)
                throw "We should have compiled a single atanh for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueATanhForSideEffects(argument) {
    return Math.atanh(argument);
}
noInline(opaqueATanhForSideEffects);
noOSRExitFuzzing(opaqueATanhForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let atanhResult = Math.atanh(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueATanhForSideEffects(testObject) !== atanhResult)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueATanhForSideEffects) > 1)
        throw "opaqueATanhForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify atanh() is not subject to CSE if the argument has side effects.
function opaqueATanhForCSE(argument) {
    return Math.atanh(argument) + Math.atanh(argument) + Math.atanh(argument);
}
noInline(opaqueATanhForCSE);
noOSRExitFuzzing(opaqueATanhForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let atanhResult = Math.atanh(0.2);
    let threeatanhResult = atanhResult + atanhResult + atanhResult;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueATanhForCSE(testObject) !== threeatanhResult)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueATanhForCSE) > 1)
        throw "opaqueATanhForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify atanh() is not subject to DCE if the argument has side effects.
function opaqueATanhForDCE(argument) {
    Math.atanh(argument);
}
noInline(opaqueATanhForDCE);
noOSRExitFuzzing(opaqueATanhForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueATanhForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueATanhForDCE) > 1)
        throw "opaqueATanhForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueATanhWithException(argument) {
        let result = Math.atanh(argument);
        ++counter;
        return result;
    }
    noInline(opaqueATanhWithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let atanhResult = Math.atanh(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueATanhWithException(testObject) !== atanhResult)
            throw "Incorrect result in opaqueATanhWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueATanhWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueATanhWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
