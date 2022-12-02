//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let log2OfHalf = Math.log2(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "-Infinity"],
    ["1", "0"],
    ["0", "-Infinity"],
    ["-0.", "-Infinity"],
    ["0.5", "" + log2OfHalf],
    ["Math.PI", "" + Math.log2(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + log2OfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + log2OfHalf],
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


// Test Math.log2() without arguments.
function opaqueLog2NoArgument() {
    return Math.log2();
}
noInline(opaqueLog2NoArgument);
noOSRExitFuzzing(opaqueLog2NoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueLog2NoArgument();
        if (output === output) {
            throw "Failed opaqueLog2NoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueLog2NoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.log2() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesLog2(argument) {
    return Math.log2(argument);
}
noInline(opaqueAllTypesLog2);
noOSRExitFuzzing(opaqueAllTypesLog2);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesLog2(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesLog2) > 2)
        throw "We should have detected log2() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.log2() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueLog2(argument) {
                return Math.log2(argument);
            }
            noInline(opaqueLog2);
            noOSRExitFuzzing(opaqueLog2);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueLog2(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueLog2) > 1)
                throw "We should have compiled a single log2 for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.log2() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueLog2OnConstant() {
                return Math.log2(${testCaseInput[0]});
            }
            noInline(opaqueLog2OnConstant);
            noOSRExitFuzzing(opaqueLog2OnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueLog2OnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueLog2OnConstant) > 1)
                throw "We should have compiled a single log2 for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueLog2ForSideEffects(argument) {
    return Math.log2(argument);
}
noInline(opaqueLog2ForSideEffects);
noOSRExitFuzzing(opaqueLog2ForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let log2Result = Math.log2(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueLog2ForSideEffects(testObject) !== log2Result)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueLog2ForSideEffects) > 1)
        throw "opaqueLog2ForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify log2() is not subject to CSE if the argument has side effects.
function opaqueLog2ForCSE(argument) {
    return Math.log2(argument) + Math.log2(argument) + Math.log2(argument);
}
noInline(opaqueLog2ForCSE);
noOSRExitFuzzing(opaqueLog2ForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let log2Result = Math.log2(0.2);
    let threelog2Result = log2Result + log2Result + log2Result;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueLog2ForCSE(testObject) !== threelog2Result)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueLog2ForCSE) > 1)
        throw "opaqueLog2ForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify log2() is not subject to DCE if the argument has side effects.
function opaqueLog2ForDCE(argument) {
    Math.log2(argument);
}
noInline(opaqueLog2ForDCE);
noOSRExitFuzzing(opaqueLog2ForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueLog2ForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueLog2ForDCE) > 1)
        throw "opaqueLog2ForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueLog2WithException(argument) {
        let result = Math.log2(argument);
        ++counter;
        return result;
    }
    noInline(opaqueLog2WithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let log2Result = Math.log2(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueLog2WithException(testObject) !== log2Result)
            throw "Incorrect result in opaqueLog2WithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueLog2WithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueLog2WithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
