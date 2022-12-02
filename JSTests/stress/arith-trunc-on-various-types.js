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
    ["0.5", "0"],
    ["-0.5", "-0"],
    ["4", "4"],
    ["42.1", "42"],
    ["42.5", "42"],
    ["42.9", "42"],
    ["-42.1", "-42"],
    ["-42.5", "-42"],
    ["-42.9", "-42"],
    ["Math.PI", "3"],
    ["Infinity", "Infinity"],
    ["-Infinity", "-Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "4"],
    ["\"42.5\"", "42"],
    ["{ valueOf: () => { return 4; } }", "4"],
    ["{ valueOf: () => { return 0; } }", "0"],
    ["{ valueOf: () => { return -0; } }", "-0"],
    ["{ valueOf: () => { return 0.5; } }", "0"],
    ["{ valueOf: () => { return -0.5; } }", "-0"],
    ["{ valueOf: () => { return Number.MIN_SAFE_INTEGER; } }", "-9007199254740991"],
    ["{ valueOf: () => { return Number.MAX_SAFE_INTEGER; } }", "9007199254740991"],
    ["{ valueOf: () => { return 0x80000000|0; } }", "-2147483648"],
    ["{ valueOf: () => { return 0x7fffffff|0; } }", "2147483647"],
    ["{ valueOf: () => { return (0x80000000|0) - 0.5; } }", "-2147483648"],
    ["{ valueOf: () => { return (0x7fffffff|0) + 0.5; } }", "2147483647"],
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


// Test Math.trunc() without arguments.
function opaqueTruncNoArgument() {
    return Math.trunc();
}
noInline(opaqueTruncNoArgument);
noOSRExitFuzzing(opaqueTruncNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueTruncNoArgument();
        if (!isIdentical(output, NaN)) {
            throw "Failed opaqueTruncNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueTruncNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.trunc() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesTrunc(argument) {
    return Math.trunc(argument);
}
noInline(opaqueAllTypesTrunc);
noOSRExitFuzzing(opaqueAllTypesTrunc);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesTrunc(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesTrunc) > 3)
        throw "We should have detected trunc() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Polymorphic input but negative zero is not observable.
function opaqueAllTypesTruncWithoutNegativeZero(argument) {
    return Math.trunc(argument) + 0;
}
noInline(opaqueAllTypesTruncWithoutNegativeZero);
noOSRExitFuzzing(opaqueAllTypesTruncWithoutNegativeZero);

function testAllTypesWithoutNegativeZeroCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesTruncWithoutNegativeZero(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1] + 0))
                throw "Failed testAllTypesWithoutNegativeZeroCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesTrunc) > 3)
        throw "We should have detected trunc() was polymorphic and generated a generic version.";
}
testAllTypesWithoutNegativeZeroCall();


// Test Math.trunc() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTrunc(argument) {
                return Math.trunc(argument);
            }
            noInline(opaqueTrunc);
            noOSRExitFuzzing(opaqueTrunc);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTrunc(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueTrunc) > 1)
                throw "Failed testSingleTypeCall(). We should have compiled a single trunc for the expected type.";
        `);
    }
}
testSingleTypeCall();


function checkCompileCountForUselessNegativeZero(testFunction)
{
    return numberOfDFGCompiles(testFunction) <= 1;
}


// Test Math.trunc() on a completely typed input, but without negative zero.
function testSingleTypeWithoutNegativeZeroCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTrunc(argument) {
                return Math.trunc(argument) + 0;
            }
            noInline(opaqueTrunc);
            noOSRExitFuzzing(opaqueTrunc);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTrunc(${testCaseInput[0]}), ${testCaseInput[1]} + 0)) {
                    throw "Failed testSingleTypeWithoutNegativeZeroCall()";
                }
            }
            if (!checkCompileCountForUselessNegativeZero(opaqueTrunc))
                throw "Failed testSingleTypeWithoutNegativeZeroCall(). We should have compiled a single trunc for the expected type.";
        `);
    }
}
testSingleTypeWithoutNegativeZeroCall();


// Test Math.trunc() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueTruncOnConstant() {
                return Math.trunc(${testCaseInput[0]});
            }
            noInline(opaqueTruncOnConstant);
            noOSRExitFuzzing(opaqueTruncOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueTruncOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueTruncOnConstant) > 1)
                throw "Failed testConstant(). We should have compiled a single trunc for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueTruncForSideEffects(argument) {
    return Math.trunc(argument);
}
noInline(opaqueTruncForSideEffects);
noOSRExitFuzzing(opaqueTruncForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let trunc16 = Math.trunc(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueTruncForSideEffects(testObject) !== trunc16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueTruncForSideEffects) > 1)
        throw "opaqueTruncForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify trunc() is not subject to CSE if the argument has side effects.
function opaqueTruncForCSE(argument) {
    return Math.trunc(argument) + Math.trunc(argument) + Math.trunc(argument);
}
noInline(opaqueTruncForCSE);
noOSRExitFuzzing(opaqueTruncForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let trunc16 = Math.trunc(16);
    let threeTrunc16 = trunc16 + trunc16 + trunc16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueTruncForCSE(testObject) !== threeTrunc16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueTruncForCSE) > 1)
        throw "opaqueTruncForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify trunc() is not subject to DCE if the argument has side effects.
function opaqueTruncForDCE(argument) {
    Math.trunc(argument);
}
noInline(opaqueTruncForDCE);
noOSRExitFuzzing(opaqueTruncForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueTruncForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueTruncForDCE) > 1)
        throw "opaqueTruncForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueTruncWithException(argument) {
        let result = Math.trunc(argument);
        ++counter;
        return result;
    }
    noInline(opaqueTruncWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let trunc64 = Math.trunc(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueTruncWithException(testObject) !== trunc64)
            throw "Incorrect result in opaqueTruncWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueTruncWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueTruncWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
