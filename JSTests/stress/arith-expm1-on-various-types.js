//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let expm1OfHalf = Math.expm1(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["1", "" + Math.expm1(1)],
    ["0", "0"],
    ["-0.", "-0"],
    ["0.5", "" + expm1OfHalf],
    ["Math.PI", "" + Math.expm1(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "-1"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + expm1OfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + expm1OfHalf],
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


// Test Math.expm1() without arguments.
function opaqueExpm1NoArgument() {
    return Math.expm1();
}
noInline(opaqueExpm1NoArgument);
noOSRExitFuzzing(opaqueExpm1NoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueExpm1NoArgument();
        if (output === output) {
            throw "Failed opaqueExpm1NoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueExpm1NoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.expm1() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesExpm1(argument) {
    return Math.expm1(argument);
}
noInline(opaqueAllTypesExpm1);
noOSRExitFuzzing(opaqueAllTypesExpm1);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesExpm1(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesExpm1) > 2)
        throw "We should have detected expm1() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.expm1() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueExpm1(argument) {
                return Math.expm1(argument);
            }
            noInline(opaqueExpm1);
            noOSRExitFuzzing(opaqueExpm1);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueExpm1(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueExpm1) > 1)
                throw "We should have compiled a single expm1 for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.expm1() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueExpm1OnConstant() {
                return Math.expm1(${testCaseInput[0]});
            }
            noInline(opaqueExpm1OnConstant);
            noOSRExitFuzzing(opaqueExpm1OnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueExpm1OnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueExpm1OnConstant) > 1)
                throw "We should have compiled a single expm1 for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueExpm1ForSideEffects(argument) {
    return Math.expm1(argument);
}
noInline(opaqueExpm1ForSideEffects);
noOSRExitFuzzing(opaqueExpm1ForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let expm1Result = Math.expm1(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueExpm1ForSideEffects(testObject) !== expm1Result)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueExpm1ForSideEffects) > 1)
        throw "opaqueExpm1ForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify expm1() is not subject to CSE if the argument has side effects.
function opaqueExpm1ForCSE(argument) {
    return Math.expm1(argument) + Math.expm1(argument) + Math.expm1(argument);
}
noInline(opaqueExpm1ForCSE);
noOSRExitFuzzing(opaqueExpm1ForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let expm1Result = Math.expm1(0.2);
    let threeexpm1Result = expm1Result + expm1Result + expm1Result;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueExpm1ForCSE(testObject) !== threeexpm1Result)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueExpm1ForCSE) > 1)
        throw "opaqueExpm1ForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify expm1() is not subject to DCE if the argument has side effects.
function opaqueExpm1ForDCE(argument) {
    Math.expm1(argument);
}
noInline(opaqueExpm1ForDCE);
noOSRExitFuzzing(opaqueExpm1ForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueExpm1ForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueExpm1ForDCE) > 1)
        throw "opaqueExpm1ForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueExpm1WithException(argument) {
        let result = Math.expm1(argument);
        ++counter;
        return result;
    }
    noInline(opaqueExpm1WithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let expm1Result = Math.expm1(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueExpm1WithException(testObject) !== expm1Result)
            throw "Incorrect result in opaqueExpm1WithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueExpm1WithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueExpm1WithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
