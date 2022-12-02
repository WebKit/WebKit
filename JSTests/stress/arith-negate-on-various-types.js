//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ defaultNoEagerRun
"use strict";

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "NaN"],
    ["null", "-0"],
    ["0", "-0"],
    ["-0.", "0"],
    ["0.5", "-0.5"],
    ["-0.5", "0.5"],
    ["4", "-4"],
    ["42.1", "-42.1"],
    ["42.5", "-42.5"],
    ["42.9", "-42.9"],
    ["-42.1", "42.1"],
    ["-42.5", "42.5"],
    ["-42.9", "42.9"],
    ["Math.PI", "-Math.PI"],
    ["Infinity", "-Infinity"],
    ["-Infinity", "Infinity"],
    ["NaN", "NaN"],
    ["\"WebKit\"", "NaN"],
    ["\"4\"", "-4"],
    ["\"42.5\"", "-42.5"],
    ["{ valueOf: () => { return 4; } }", "-4"],
    ["{ valueOf: () => { return 0; } }", "-0"],
    ["{ valueOf: () => { return -0; } }", "0"],
    ["{ valueOf: () => { return 0.5; } }", "-0.5"],
    ["{ valueOf: () => { return -0.5; } }", "0.5"],
    ["{ valueOf: () => { return Number.MIN_SAFE_INTEGER; } }", "9007199254740991"],
    ["{ valueOf: () => { return Number.MAX_SAFE_INTEGER; } }", "-9007199254740991"],
    ["{ valueOf: () => { return 0x80000000|0; } }", "2147483648"],
    ["{ valueOf: () => { return 0x7fffffff|0; } }", "-2147483647"],
    ["{ valueOf: () => { return (0x80000000|0) - 0.5; } }", "2147483648.5"],
    ["{ valueOf: () => { return (0x7fffffff|0) + 0.5; } }", "-2147483647.5"],
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


// Test negate with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesNegate(argument) {
    return -(argument);
}
noInline(opaqueAllTypesNegate);
noOSRExitFuzzing(opaqueAllTypesNegate);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesNegate(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesNegate) > 3)
        throw "We should have detected negate was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Polymorphic input but negative zero is not observable.
function opaqueAllTypesNegateWithoutNegativeZero(argument) {
    return -(argument) + 0;
}
noInline(opaqueAllTypesNegateWithoutNegativeZero);
noOSRExitFuzzing(opaqueAllTypesNegateWithoutNegativeZero);

function testAllTypesWithoutNegativeZeroCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesNegateWithoutNegativeZero(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1] + 0))
                throw "Failed testAllTypesWithoutNegativeZeroCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesNegate) > 3)
        throw "We should have detected negate was polymorphic and generated a generic version.";
}
testAllTypesWithoutNegativeZeroCall();


// Test negate on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueNegate(argument) {
                return -(argument);
            }
            noInline(opaqueNegate);
            noOSRExitFuzzing(opaqueNegate);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueNegate(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueNegate) > 1)
                throw "Failed testSingleTypeCall(). We should have compiled a single negate for the expected type.";
        `);
    }
}
testSingleTypeCall();


function checkCompileCountForUselessNegativeZero(testFunction)
{
    return numberOfDFGCompiles(testFunction) <= 1;
}


// Test negate on a completely typed input, but without negative zero.
function testSingleTypeWithoutNegativeZeroCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueNegate(argument) {
                return -(argument) + 0;
            }
            noInline(opaqueNegate);
            noOSRExitFuzzing(opaqueNegate);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueNegate(${testCaseInput[0]}), ${testCaseInput[1]} + 0)) {
                    throw "Failed testSingleTypeWithoutNegativeZeroCall()";
                }
            }
            if (!checkCompileCountForUselessNegativeZero(opaqueNegate))
                throw "Failed testSingleTypeWithoutNegativeZeroCall(). We should have compiled a single negate for the expected type.";
        `);
    }
}
testSingleTypeWithoutNegativeZeroCall();


// Test negate on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueNegateOnConstant() {
                return -(${testCaseInput[0]});
            }
            noInline(opaqueNegateOnConstant);
            noOSRExitFuzzing(opaqueNegateOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueNegateOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueNegateOnConstant) > 1)
                throw "Failed testConstant(). We should have compiled a single negate for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueNegateForSideEffects(argument) {
    return -(argument);
}
noInline(opaqueNegateForSideEffects);
noOSRExitFuzzing(opaqueNegateForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueNegateForSideEffects(testObject) !== -16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueNegateForSideEffects) > 1)
        throw "opaqueNegateForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify negate is not subject to CSE if the argument has side effects.
function opaqueNegateForCSE(argument) {
    return -(argument) + -(argument) + -(argument);
}
noInline(opaqueNegateForCSE);
noOSRExitFuzzing(opaqueNegateForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueNegateForCSE(testObject) !== -48)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueNegateForCSE) > 1)
        throw "opaqueNegateForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify negate is not subject to DCE if the argument has side effects.
function opaqueNegateForDCE(argument) {
    -(argument);
}
noInline(opaqueNegateForDCE);
noOSRExitFuzzing(opaqueNegateForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueNegateForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueNegateForDCE) > 1)
        throw "opaqueNegateForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueNegateWithException(argument) {
        let result = -(argument);
        ++counter;
        return result;
    }
    noInline(opaqueNegateWithException);

    let testObject = { valueOf: () => {  return 64; } };

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueNegateWithException(testObject) !== -64)
            throw "Incorrect result in opaqueNegateWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueNegateWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueNegateWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
