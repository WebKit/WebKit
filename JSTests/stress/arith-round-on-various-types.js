//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "0"],
    ["0", "0"],
    ["-0.", "-0"],
    ["0.5", "1"],
    ["-0.5", "-0"],
    ["4", "4"],
    ["42.1", "42"],
    ["42.5", "43"],
    ["42.9", "43"],
    ["-42.1", "-42"],
    ["-42.5", "-42"],
    ["-42.9", "-43"],
    ["Math.PI", "3"],
    ["Infinity", "Infinity"],
    ["-Infinity", "-Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "4"],
    ["\"42.5\"", "43"],
    ["{ valueOf: () => { return 4; } }", "4"],
    ["{ valueOf: () => { return 0; } }", "0"],
    ["{ valueOf: () => { return -0; } }", "-0"],
    ["{ valueOf: () => { return 0.5; } }", "1"],
    ["{ valueOf: () => { return -0.5; } }", "-0"],
    ["{ valueOf: () => { return Number.MIN_SAFE_INTEGER; } }", "-9007199254740991"],
    ["{ valueOf: () => { return Number.MAX_SAFE_INTEGER; } }", "9007199254740991"],
    ["{ valueOf: () => { return 0x80000000|0; } }", "-2147483648"],
    ["{ valueOf: () => { return 0x7fffffff|0; } }", "2147483647"],
    ["{ valueOf: () => { return (0x80000000|0) - 0.5; } }", "-2147483648"],
    ["{ valueOf: () => { return (0x7fffffff|0) + 0.5; } }", "2147483648"],
];

let validInputTypedTestCases = validInputTestCases.map((element) => { return [eval("(" + element[0] + ")"), eval(element[1])] });

function isIdentical(result, expected)
{
    if (expected === expected) {
        if (result !== expected)
            return false;
        if (!expected && (1 / expected) !== (1 / result))
            return false;

        return true;
    }
    return result !== result;
}


// Test Math.round() without arguments.
function opaqueRoundNoArgument() {
    return Math.round();
}
noInline(opaqueRoundNoArgument);
noOSRExitFuzzing(opaqueRoundNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueRoundNoArgument();
        if (!isIdentical(output, NaN)) {
            throw "Failed opaqueRoundNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueRoundNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.round() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesRound(argument) {
    return Math.round(argument);
}
noInline(opaqueAllTypesRound);
noOSRExitFuzzing(opaqueAllTypesRound);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesRound(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesRound) > 3)
        throw "We should have detected round() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Polymorphic input but negative zero is not observable.
function opaqueAllTypesRoundWithoutNegativeZero(argument) {
    return Math.round(argument) + 0;
}
noInline(opaqueAllTypesRoundWithoutNegativeZero);
noOSRExitFuzzing(opaqueAllTypesRoundWithoutNegativeZero);

function testAllTypesWithoutNegativeZeroCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesRoundWithoutNegativeZero(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1] + 0))
                throw "Failed testAllTypesWithoutNegativeZeroCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesRound) > 3)
        throw "We should have detected round() was polymorphic and generated a generic version.";
}
testAllTypesWithoutNegativeZeroCall();


// Test Math.round() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueRound(argument) {
                return Math.round(argument);
            }
            noInline(opaqueRound);
            noOSRExitFuzzing(opaqueRound);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueRound(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueRound) > 1)
                throw "We should have compiled a single round for the expected type.";
        `);
    }
}
testSingleTypeCall();


function checkCompileCountForUselessNegativeZero(testFunction)
{
    return numberOfDFGCompiles(testFunction) <= 1;
}


// Test Math.round() on a completely typed input, but without negative zero.
function testSingleTypeWithoutNegativeZeroCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueRound(argument) {
                return Math.round(argument) + 0;
            }
            noInline(opaqueRound);
            noOSRExitFuzzing(opaqueRound);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueRound(${testCaseInput[0]}), ${testCaseInput[1]} + 0)) {
                    throw "Failed testSingleTypeWithoutNegativeZeroCall()";
                }
            }
            if (!checkCompileCountForUselessNegativeZero(opaqueRound))
                throw "We should have compiled a single round for the expected type.";
        `);
    }
}
testSingleTypeWithoutNegativeZeroCall();


// Test Math.round() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueRoundOnConstant() {
                return Math.round(${testCaseInput[0]});
            }
            noInline(opaqueRoundOnConstant);
            noOSRExitFuzzing(opaqueRoundOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueRoundOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueRoundOnConstant) > 1)
                throw "We should have compiled a single round for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueRoundForSideEffects(argument) {
    return Math.round(argument);
}
noInline(opaqueRoundForSideEffects);
noOSRExitFuzzing(opaqueRoundForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let round16 = Math.round(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueRoundForSideEffects(testObject) !== round16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueRoundForSideEffects) > 1)
        throw "opaqueRoundForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify round() is not subject to CSE if the argument has side effects.
function opaqueRoundForCSE(argument) {
    return Math.round(argument) + Math.round(argument) + Math.round(argument);
}
noInline(opaqueRoundForCSE);
noOSRExitFuzzing(opaqueRoundForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let round16 = Math.round(16);
    let threeRound16 = round16 + round16 + round16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueRoundForCSE(testObject) !== threeRound16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueRoundForCSE) > 1)
        throw "opaqueRoundForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify round() is not subject to DCE if the argument has side effects.
function opaqueRoundForDCE(argument) {
    Math.round(argument);
}
noInline(opaqueRoundForDCE);
noOSRExitFuzzing(opaqueRoundForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueRoundForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueRoundForDCE) > 1)
        throw "opaqueRoundForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueRoundWithException(argument) {
        let result = Math.round(argument);
        ++counter;
        return result;
    }
    noInline(opaqueRoundWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let round64 = Math.round(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueRoundWithException(testObject) !== round64)
            throw "Incorrect result in opaqueRoundWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueRoundWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueRoundWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
