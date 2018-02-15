//@ skip if not $jitTests
//@ runNoCJITValidatePhases
//@ runFTLNoCJITValidate
"use strict";

let logOfFour = Math.log(4);

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "-Infinity"],
    ["0", "-Infinity"],
    ["-0.", "-Infinity"],
    ["1.", "0"],
    ["4", "" + logOfFour],
    ["Math.E", "1"],
    ["Infinity", "Infinity"],
    ["-Infinity", "NaN"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "" + logOfFour],
    ["{ valueOf: () => { return Math.E; } }", "1"],
    ["{ valueOf: () => { return 1; } }", "0"],
    ["{ valueOf: () => { return 4; } }", "" + logOfFour],
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


// Test Math.log() without arguments.
function opaqueLogNoArgument() {
    return Math.log();
}
noInline(opaqueLogNoArgument);
noOSRExitFuzzing(opaqueLogNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueLogNoArgument();
        if (output === output) {
            throw "Failed opaqueLogNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueLogNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.log() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesLog(argument) {
    return Math.log(argument);
}
noInline(opaqueAllTypesLog);
noOSRExitFuzzing(opaqueAllTypesLog);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesLog(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesLog) > 2)
        throw "We should have detected log() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.log() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueLog(argument) {
                return Math.log(argument);
            }
            noInline(opaqueLog);
            noOSRExitFuzzing(opaqueLog);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueLog(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueLog) > 1)
                throw "We should have compiled a single log for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.log() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueLogOnConstant() {
                return Math.log(${testCaseInput[0]});
            }
            noInline(opaqueLogOnConstant);
            noOSRExitFuzzing(opaqueLogOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueLogOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueLogOnConstant) > 1)
                throw "We should have compiled a single log for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueLogForSideEffects(argument) {
    return Math.log(argument);
}
noInline(opaqueLogForSideEffects);
noOSRExitFuzzing(opaqueLogForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let log16 = Math.log(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueLogForSideEffects(testObject) !== log16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueLogForSideEffects) > 1)
        throw "opaqueLogForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify log() is not subject to CSE if the argument has side effects.
function opaqueLogForCSE(argument) {
    return Math.log(argument) + Math.log(argument) + Math.log(argument);
}
noInline(opaqueLogForCSE);
noOSRExitFuzzing(opaqueLogForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let log16 = Math.log(16);
    let threeLog16 = log16 + log16 + log16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueLogForCSE(testObject) !== threeLog16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueLogForCSE) > 1)
        throw "opaqueLogForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify log() is not subject to DCE if the argument has side effects.
function opaqueLogForDCE(argument) {
    Math.log(argument);
}
noInline(opaqueLogForDCE);
noOSRExitFuzzing(opaqueLogForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueLogForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueLogForDCE) > 1)
        throw "opaqueLogForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueLogWithException(argument) {
        let result = Math.log(argument);
        ++counter;
        return result;
    }
    noInline(opaqueLogWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let log64 = Math.log(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueLogWithException(testObject) !== log64)
            throw "Incorrect result in opaqueLogWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueLogWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueLogWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
