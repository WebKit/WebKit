//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let froundOfPi = Math.fround(Math.PI);
let froundOfE = Math.fround(Math.E);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["1.", "1"],
    ["Math.PI", "" + froundOfPi],
    ["Math.E", "" + froundOfE],
    ["Infinity", "Infinity"],
    ["-Infinity", "-Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"" + Math.PI + "\"", "" + froundOfPi],
    ["{ valueOf: () => { return Math.E; } }", "" + froundOfE],
    ["{ valueOf: () => { return 1; } }", "1"],
    ["{ valueOf: () => { return Math.PI; } }", "" + froundOfPi],
];

let validInputTypedTestCases = validInputTestCases.map((element) => { return [eval("(" + element[0] + ")"), eval(element[1])] });

function isIdentical(result, expected)
{
    if (expected === expected) {
        if (result !== expected)
            return false;
        if (!expected && 1 / expected === -Infinity && 1 / result !== -Infinity)
            return false;

        return true;
    }
    return result !== result;
}

// Test Math.fround() without arguments.
function opaqueFRoundNoArgument() {
    return Math.fround();
}
noInline(opaqueFRoundNoArgument);
noOSRExitFuzzing(opaqueFRoundNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueFRoundNoArgument();
        if (output === output) {
            throw "Failed opaqueFRoundNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueFRoundNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();

// Test Math.fround() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesFround(argument) {
    return Math.fround(argument);
}
noInline(opaqueAllTypesFround);
noOSRExitFuzzing(opaqueAllTypesFround);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesFround(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesFround) > 2)
        throw "We should have detected fround() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.fround() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueFround(argument) {
                return Math.fround(argument);
            }
            noInline(opaqueFround);
            noOSRExitFuzzing(opaqueFround);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueFround(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueFround) > 1)
                throw "We should have compiled a single fround for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.fround() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueFroundOnConstant() {
                return Math.fround(${testCaseInput[0]});
            }
            noInline(opaqueFroundOnConstant);
            noOSRExitFuzzing(opaqueFroundOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueFroundOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueFroundOnConstant) > 1)
                throw "We should have compiled a single fround for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueFroundForSideEffects(argument) {
    return Math.fround(argument);
}
noInline(opaqueFroundForSideEffects);
noOSRExitFuzzing(opaqueFroundForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let fround16 = Math.fround(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueFroundForSideEffects(testObject) !== fround16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueFroundForSideEffects) > 1)
        throw "opaqueFroundForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify fround() is not subject to CSE if the argument has side effects.
function opaqueFroundForCSE(argument) {
    return Math.fround(argument) + Math.fround(argument) + Math.fround(argument);
}
noInline(opaqueFroundForCSE);
noOSRExitFuzzing(opaqueFroundForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let fround16 = Math.fround(16);
    let threeFround16 = fround16 + fround16 + fround16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueFroundForCSE(testObject) !== threeFround16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueFroundForCSE) > 1)
        throw "opaqueFroundForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify fround() is not subject to DCE if the argument has side effects.
function opaqueFroundForDCE(argument) {
    Math.fround(argument);
}
noInline(opaqueFroundForDCE);
noOSRExitFuzzing(opaqueFroundForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueFroundForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueFroundForDCE) > 1)
        throw "opaqueFroundForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueFroundWithException(argument) {
        let result = Math.fround(argument);
        ++counter;
        return result;
    }
    noInline(opaqueFroundWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let fround64 = Math.fround(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueFroundWithException(testObject) !== fround64)
            throw "Incorrect result in opaqueFroundWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueFroundWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueFroundWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
