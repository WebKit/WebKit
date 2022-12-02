//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let tanhOfFour = Math.tanh(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["4", "" + tanhOfFour],
    ["Math.PI", "" + Math.tanh(Math.PI)],
    ["Infinity", "1"],
    ["-Infinity", "-1"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + tanhOfFour],
    ["{ valueOf: () => { return 4; } }", "" + tanhOfFour],
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


// Test Math.tanh() without arguments.
function opaqueTanhNoArgument() {
    return Math.tanh();
}
noInline(opaqueTanhNoArgument);
noOSRExitFuzzing(opaqueTanhNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueTanhNoArgument();
        if (output === output) {
            throw "Failed opaqueTanhNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueTanhNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.tanh() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesTanh(argument) {
    return Math.tanh(argument);
}
noInline(opaqueAllTypesTanh);
noOSRExitFuzzing(opaqueAllTypesTanh);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesTanh(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesTanh) > 2)
        throw "We should have detected tanh() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.tanh() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTanh(argument) {
                return Math.tanh(argument);
            }
            noInline(opaqueTanh);
            noOSRExitFuzzing(opaqueTanh);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTanh(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueTanh) > 1)
                throw "We should have compiled a single tanh for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.tanh() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTanhOnConstant() {
                return Math.tanh(${testCaseInput[0]});
            }
            noInline(opaqueTanhOnConstant);
            noOSRExitFuzzing(opaqueTanhOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTanhOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueTanhOnConstant) > 1)
                throw "We should have compiled a single tanh for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueTanhForSideEffects(argument) {
    return Math.tanh(argument);
}
noInline(opaqueTanhForSideEffects);
noOSRExitFuzzing(opaqueTanhForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let tanh16 = Math.tanh(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueTanhForSideEffects(testObject) !== tanh16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueTanhForSideEffects) > 1)
        throw "opaqueTanhForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify tanh() is not subject to CSE if the argument has side effects.
function opaqueTanhForCSE(argument) {
    return Math.tanh(argument) + Math.tanh(argument) + Math.tanh(argument);
}
noInline(opaqueTanhForCSE);
noOSRExitFuzzing(opaqueTanhForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let tanh16 = Math.tanh(16);
    let threeTanh16 = tanh16 + tanh16 + tanh16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueTanhForCSE(testObject) !== threeTanh16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueTanhForCSE) > 1)
        throw "opaqueTanhForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify tanh() is not subject to DCE if the argument has side effects.
function opaqueTanhForDCE(argument) {
    Math.tanh(argument);
}
noInline(opaqueTanhForDCE);
noOSRExitFuzzing(opaqueTanhForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueTanhForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueTanhForDCE) > 1)
        throw "opaqueTanhForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueTanhWithException(argument) {
        let result = Math.tanh(argument);
        ++counter;
        return result;
    }
    noInline(opaqueTanhWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let tanh64 = Math.tanh(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueTanhWithException(testObject) !== tanh64)
            throw "Incorrect result in opaqueTanhWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueTanhWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueTanhWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
