//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let log10OfHalf = Math.log10(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "-Infinity"],
    ["1", "0"],
    ["0", "-Infinity"],
    ["-0.", "-Infinity"],
    ["0.5", "" + log10OfHalf],
    ["Math.PI", "" + Math.log10(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + log10OfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + log10OfHalf],
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


// Test Math.log10() without arguments.
function opaqueLog10NoArgument() {
    return Math.log10();
}
noInline(opaqueLog10NoArgument);
noOSRExitFuzzing(opaqueLog10NoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueLog10NoArgument();
        if (output === output) {
            throw "Failed opaqueLog10NoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueLog10NoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.log10() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesLog10(argument) {
    return Math.log10(argument);
}
noInline(opaqueAllTypesLog10);
noOSRExitFuzzing(opaqueAllTypesLog10);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesLog10(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesLog10) > 2)
        throw "We should have detected log10() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.log10() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueLog10(argument) {
                return Math.log10(argument);
            }
            noInline(opaqueLog10);
            noOSRExitFuzzing(opaqueLog10);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueLog10(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueLog10) > 1)
                throw "We should have compiled a single log10 for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.log10() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueLog10OnConstant() {
                return Math.log10(${testCaseInput[0]});
            }
            noInline(opaqueLog10OnConstant);
            noOSRExitFuzzing(opaqueLog10OnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueLog10OnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueLog10OnConstant) > 1)
                throw "We should have compiled a single log10 for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueLog10ForSideEffects(argument) {
    return Math.log10(argument);
}
noInline(opaqueLog10ForSideEffects);
noOSRExitFuzzing(opaqueLog10ForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let log10Result = Math.log10(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueLog10ForSideEffects(testObject) !== log10Result)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueLog10ForSideEffects) > 1)
        throw "opaqueLog10ForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify log10() is not subject to CSE if the argument has side effects.
function opaqueLog10ForCSE(argument) {
    return Math.log10(argument) + Math.log10(argument) + Math.log10(argument);
}
noInline(opaqueLog10ForCSE);
noOSRExitFuzzing(opaqueLog10ForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let log10Result = Math.log10(0.2);
    let threelog10Result = log10Result + log10Result + log10Result;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueLog10ForCSE(testObject) !== threelog10Result)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueLog10ForCSE) > 1)
        throw "opaqueLog10ForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify log10() is not subject to DCE if the argument has side effects.
function opaqueLog10ForDCE(argument) {
    Math.log10(argument);
}
noInline(opaqueLog10ForDCE);
noOSRExitFuzzing(opaqueLog10ForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueLog10ForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueLog10ForDCE) > 1)
        throw "opaqueLog10ForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueLog10WithException(argument) {
        let result = Math.log10(argument);
        ++counter;
        return result;
    }
    noInline(opaqueLog10WithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let log10Result = Math.log10(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueLog10WithException(testObject) !== log10Result)
            throw "Incorrect result in opaqueLog10WithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueLog10WithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueLog10WithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
