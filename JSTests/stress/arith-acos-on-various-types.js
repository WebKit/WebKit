//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let acosOfHalf = Math.acos(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "" + Math.acos(0)],
    ["1", "0"],
    ["0", "" + Math.acos(0)],
    ["-0.", "" + Math.acos(0)],
    ["0.5", "" + acosOfHalf],
    ["Math.PI", "" + Math.acos(Math.PI)],
    ["Infinity", "NaN"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + acosOfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + acosOfHalf],
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


// Test Math.acos() without arguments.
function opaqueACosNoArgument() {
    return Math.acos();
}
noInline(opaqueACosNoArgument);
noOSRExitFuzzing(opaqueACosNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueACosNoArgument();
        if (output === output) {
            throw "Failed opaqueACosNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueACosNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.acos() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesACos(argument) {
    return Math.acos(argument);
}
noInline(opaqueAllTypesACos);
noOSRExitFuzzing(opaqueAllTypesACos);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesACos(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesACos) > 2)
        throw "We should have detected acos() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.acos() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueACos(argument) {
                return Math.acos(argument);
            }
            noInline(opaqueACos);
            noOSRExitFuzzing(opaqueACos);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueACos(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueACos) > 1)
                throw "We should have compiled a single acos for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.acos() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueACosOnConstant() {
                return Math.acos(${testCaseInput[0]});
            }
            noInline(opaqueACosOnConstant);
            noOSRExitFuzzing(opaqueACosOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueACosOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueACosOnConstant) > 1)
                throw "We should have compiled a single acos for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueACosForSideEffects(argument) {
    return Math.acos(argument);
}
noInline(opaqueACosForSideEffects);
noOSRExitFuzzing(opaqueACosForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let acosResult = Math.acos(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueACosForSideEffects(testObject) !== acosResult)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueACosForSideEffects) > 1)
        throw "opaqueACosForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify acos() is not subject to CSE if the argument has side effects.
function opaqueACosForCSE(argument) {
    return Math.acos(argument) + Math.acos(argument) + Math.acos(argument);
}
noInline(opaqueACosForCSE);
noOSRExitFuzzing(opaqueACosForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let acosResult = Math.acos(0.2);
    let threeacosResult = acosResult + acosResult + acosResult;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueACosForCSE(testObject) !== threeacosResult)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueACosForCSE) > 1)
        throw "opaqueACosForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify acos() is not subject to DCE if the argument has side effects.
function opaqueACosForDCE(argument) {
    Math.acos(argument);
}
noInline(opaqueACosForDCE);
noOSRExitFuzzing(opaqueACosForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueACosForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueACosForDCE) > 1)
        throw "opaqueACosForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueACosWithException(argument) {
        let result = Math.acos(argument);
        ++counter;
        return result;
    }
    noInline(opaqueACosWithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let acosResult = Math.acos(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueACosWithException(testObject) !== acosResult)
            throw "Incorrect result in opaqueACosWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueACosWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueACosWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
