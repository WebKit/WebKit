//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let sinhOfFour = Math.sinh(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["4", "" + sinhOfFour],
    ["Math.PI", "" + Math.sinh(Math.PI)],
    ["Infinity", "Infinity"],
    ["-Infinity", "-Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + sinhOfFour],
    ["{ valueOf: () => { return 4; } }", "" + sinhOfFour],
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


// Test Math.sinh() without arguments.
function opaqueSinhNoArgument() {
    return Math.sinh();
}
noInline(opaqueSinhNoArgument);
noOSRExitFuzzing(opaqueSinhNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueSinhNoArgument();
        if (output === output) {
            throw "Failed opaqueSinhNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueSinhNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.sinh() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesSinh(argument) {
    return Math.sinh(argument);
}
noInline(opaqueAllTypesSinh);
noOSRExitFuzzing(opaqueAllTypesSinh);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesSinh(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesSinh) > 2)
        throw "We should have detected sinh() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.sinh() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueSinh(argument) {
                return Math.sinh(argument);
            }
            noInline(opaqueSinh);
            noOSRExitFuzzing(opaqueSinh);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueSinh(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueSinh) > 1)
                throw "We should have compiled a single sinh for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.sinh() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueSinhOnConstant() {
                return Math.sinh(${testCaseInput[0]});
            }
            noInline(opaqueSinhOnConstant);
            noOSRExitFuzzing(opaqueSinhOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueSinhOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueSinhOnConstant) > 1)
                throw "We should have compiled a single sinh for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueSinhForSideEffects(argument) {
    return Math.sinh(argument);
}
noInline(opaqueSinhForSideEffects);
noOSRExitFuzzing(opaqueSinhForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let sinh16 = Math.sinh(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueSinhForSideEffects(testObject) !== sinh16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueSinhForSideEffects) > 1)
        throw "opaqueSinhForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify sinh() is not subject to CSE if the argument has side effects.
function opaqueSinhForCSE(argument) {
    return Math.sinh(argument) + Math.sinh(argument) + Math.sinh(argument);
}
noInline(opaqueSinhForCSE);
noOSRExitFuzzing(opaqueSinhForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let sinh16 = Math.sinh(16);
    let threeSinh16 = sinh16 + sinh16 + sinh16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueSinhForCSE(testObject) !== threeSinh16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueSinhForCSE) > 1)
        throw "opaqueSinhForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify sinh() is not subject to DCE if the argument has side effects.
function opaqueSinhForDCE(argument) {
    Math.sinh(argument);
}
noInline(opaqueSinhForDCE);
noOSRExitFuzzing(opaqueSinhForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueSinhForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueSinhForDCE) > 1)
        throw "opaqueSinhForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueSinhWithException(argument) {
        let result = Math.sinh(argument);
        ++counter;
        return result;
    }
    noInline(opaqueSinhWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let sinh64 = Math.sinh(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueSinhWithException(testObject) !== sinh64)
            throw "Incorrect result in opaqueSinhWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueSinhWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueSinhWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
