//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let asinhOfFour = Math.asinh(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["4", "" + asinhOfFour],
    ["Math.PI", "" + Math.asinh(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "-Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + asinhOfFour],
    ["{ valueOf: () => { return 4; } }", "" + asinhOfFour],
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


// Test Math.asinh() without arguments.
function opaqueASinhNoArgument() {
    return Math.asinh();
}
noInline(opaqueASinhNoArgument);
noOSRExitFuzzing(opaqueASinhNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueASinhNoArgument();
        if (output === output) {
            throw "Failed opaqueASinhNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueASinhNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.asinh() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesASinh(argument) {
    return Math.asinh(argument);
}
noInline(opaqueAllTypesASinh);
noOSRExitFuzzing(opaqueAllTypesASinh);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesASinh(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesASinh) > 2)
        throw "We should have detected asinh() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.asinh() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueASinh(argument) {
                return Math.asinh(argument);
            }
            noInline(opaqueASinh);
            noOSRExitFuzzing(opaqueASinh);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueASinh(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueASinh) > 1)
                throw "We should have compiled a single asinh for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.asinh() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueASinhOnConstant() {
                return Math.asinh(${testCaseInput[0]});
            }
            noInline(opaqueASinhOnConstant);
            noOSRExitFuzzing(opaqueASinhOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueASinhOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueASinhOnConstant) > 1)
                throw "We should have compiled a single asinh for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueASinhForSideEffects(argument) {
    return Math.asinh(argument);
}
noInline(opaqueASinhForSideEffects);
noOSRExitFuzzing(opaqueASinhForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let asinh16 = Math.asinh(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueASinhForSideEffects(testObject) !== asinh16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueASinhForSideEffects) > 1)
        throw "opaqueASinhForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify asinh() is not subject to CSE if the argument has side effects.
function opaqueASinhForCSE(argument) {
    return Math.asinh(argument) + Math.asinh(argument) + Math.asinh(argument);
}
noInline(opaqueASinhForCSE);
noOSRExitFuzzing(opaqueASinhForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let asinh16 = Math.asinh(16);
    let threeASinh16 = asinh16 + asinh16 + asinh16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueASinhForCSE(testObject) !== threeASinh16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueASinhForCSE) > 1)
        throw "opaqueASinhForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify asinh() is not subject to DCE if the argument has side effects.
function opaqueASinhForDCE(argument) {
    Math.asinh(argument);
}
noInline(opaqueASinhForDCE);
noOSRExitFuzzing(opaqueASinhForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueASinhForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueASinhForDCE) > 1)
        throw "opaqueASinhForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueASinhWithException(argument) {
        let result = Math.asinh(argument);
        ++counter;
        return result;
    }
    noInline(opaqueASinhWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let asinh64 = Math.asinh(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueASinhWithException(testObject) !== asinh64)
            throw "Incorrect result in opaqueASinhWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueASinhWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueASinhWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
