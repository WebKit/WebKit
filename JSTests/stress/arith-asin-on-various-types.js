//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let asinOfHalf = Math.asin(0.5);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["1", "" + Math.asin(1)],
    ["0", "0"],
    ["-0.", "-0"],
    ["0.5", "" + asinOfHalf],
    ["Math.PI", "" + Math.asin(Math.PI)],
    ["Infinity", "NaN"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"0.5\"", "" + asinOfHalf],
    ["{ valueOf: () => { return 0.5; } }", "" + asinOfHalf],
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


// Test Math.asin() without arguments.
function opaqueASinNoArgument() {
    return Math.asin();
}
noInline(opaqueASinNoArgument);
noOSRExitFuzzing(opaqueASinNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueASinNoArgument();
        if (output === output) {
            throw "Failed opaqueASinNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueASinNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.asin() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesASin(argument) {
    return Math.asin(argument);
}
noInline(opaqueAllTypesASin);
noOSRExitFuzzing(opaqueAllTypesASin);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesASin(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesASin) > 2)
        throw "We should have detected asin() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.asin() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueASin(argument) {
                return Math.asin(argument);
            }
            noInline(opaqueASin);
            noOSRExitFuzzing(opaqueASin);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueASin(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueASin) > 1)
                throw "We should have compiled a single asin for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.asin() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueASinOnConstant() {
                return Math.asin(${testCaseInput[0]});
            }
            noInline(opaqueASinOnConstant);
            noOSRExitFuzzing(opaqueASinOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueASinOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueASinOnConstant) > 1)
                throw "We should have compiled a single asin for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueASinForSideEffects(argument) {
    return Math.asin(argument);
}
noInline(opaqueASinForSideEffects);
noOSRExitFuzzing(opaqueASinForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let asinResult = Math.asin(0.2);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueASinForSideEffects(testObject) !== asinResult)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueASinForSideEffects) > 1)
        throw "opaqueASinForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify asin() is not subject to CSE if the argument has side effects.
function opaqueASinForCSE(argument) {
    return Math.asin(argument) + Math.asin(argument) + Math.asin(argument);
}
noInline(opaqueASinForCSE);
noOSRExitFuzzing(opaqueASinForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    let asinResult = Math.asin(0.2);
    let threeasinResult = asinResult + asinResult + asinResult;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueASinForCSE(testObject) !== threeasinResult)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueASinForCSE) > 1)
        throw "opaqueASinForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify asin() is not subject to DCE if the argument has side effects.
function opaqueASinForDCE(argument) {
    Math.asin(argument);
}
noInline(opaqueASinForDCE);
noOSRExitFuzzing(opaqueASinForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 0.2; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueASinForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueASinForDCE) > 1)
        throw "opaqueASinForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueASinWithException(argument) {
        let result = Math.asin(argument);
        ++counter;
        return result;
    }
    noInline(opaqueASinWithException);

    let testObject = { valueOf: () => {  return 0.1; } };
    let asinResult = Math.asin(0.1);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueASinWithException(testObject) !== asinResult)
            throw "Incorrect result in opaqueASinWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 0.1; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueASinWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueASinWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
