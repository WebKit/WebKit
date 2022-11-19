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
    ["42.1", "43"],
    ["42.5", "43"],
    ["42.9", "43"],
    ["-42.1", "-42"],
    ["-42.5", "-42"],
    ["-42.9", "-42"],
    ["Math.PI", "4"],
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


// Test Math.ceil() without arguments.
function opaqueCeilNoArgument() {
    return Math.ceil();
}
noInline(opaqueCeilNoArgument);
noOSRExitFuzzing(opaqueCeilNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueCeilNoArgument();
        if (!isIdentical(output, NaN)) {
            throw "Failed opaqueCeilNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueCeilNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.ceil() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesCeil(argument) {
    return Math.ceil(argument);
}
noInline(opaqueAllTypesCeil);
noOSRExitFuzzing(opaqueAllTypesCeil);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesCeil(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesCeil) > 3)
        throw "We should have detected ceil() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Polymorphic input but negative zero is not observable.
function opaqueAllTypesCeilWithoutNegativeZero(argument) {
    return Math.ceil(argument) + 0;
}
noInline(opaqueAllTypesCeilWithoutNegativeZero);
noOSRExitFuzzing(opaqueAllTypesCeilWithoutNegativeZero);

function testAllTypesWithoutNegativeZeroCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesCeilWithoutNegativeZero(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1] + 0))
                throw "Failed testAllTypesWithoutNegativeZeroCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesCeil) > 3)
        throw "We should have detected ceil() was polymorphic and generated a generic version.";
}
testAllTypesWithoutNegativeZeroCall();


// Test Math.ceil() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCeil(argument) {
                return Math.ceil(argument);
            }
            noInline(opaqueCeil);
            noOSRExitFuzzing(opaqueCeil);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCeil(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueCeil) > 1)
                throw "We should have compiled a single ceil for the expected type.";
        `);
    }
}
testSingleTypeCall();


function checkCompileCountForUselessNegativeZero(testFunction)
{
    return numberOfDFGCompiles(testFunction) <= 1;
}


// Test Math.ceil() on a completely typed input, but without negative zero.
function testSingleTypeWithoutNegativeZeroCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCeil(argument) {
                return Math.ceil(argument) + 0;
            }
            noInline(opaqueCeil);
            noOSRExitFuzzing(opaqueCeil);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCeil(${testCaseInput[0]}), ${testCaseInput[1]} + 0)) {
                    throw "Failed testSingleTypeWithoutNegativeZeroCall()";
                }
            }
            if (!checkCompileCountForUselessNegativeZero(opaqueCeil))
                throw "We should have compiled a single ceil for the expected type.";
        `);
    }
}
testSingleTypeWithoutNegativeZeroCall();


// Test Math.ceil() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueCeilOnConstant() {
                return Math.ceil(${testCaseInput[0]});
            }
            noInline(opaqueCeilOnConstant);
            noOSRExitFuzzing(opaqueCeilOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueCeilOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueCeilOnConstant) > 1)
                throw "We should have compiled a single ceil for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueCeilForSideEffects(argument) {
    return Math.ceil(argument);
}
noInline(opaqueCeilForSideEffects);
noOSRExitFuzzing(opaqueCeilForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let ceil16 = Math.ceil(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCeilForSideEffects(testObject) !== ceil16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueCeilForSideEffects) > 1)
        throw "opaqueCeilForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify ceil() is not subject to CSE if the argument has side effects.
function opaqueCeilForCSE(argument) {
    return Math.ceil(argument) + Math.ceil(argument) + Math.ceil(argument);
}
noInline(opaqueCeilForCSE);
noOSRExitFuzzing(opaqueCeilForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let ceil16 = Math.ceil(16);
    let threeCeil16 = ceil16 + ceil16 + ceil16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueCeilForCSE(testObject) !== threeCeil16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueCeilForCSE) > 1)
        throw "opaqueCeilForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify ceil() is not subject to DCE if the argument has side effects.
function opaqueCeilForDCE(argument) {
    Math.ceil(argument);
}
noInline(opaqueCeilForDCE);
noOSRExitFuzzing(opaqueCeilForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueCeilForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueCeilForDCE) > 1)
        throw "opaqueCeilForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueCeilWithException(argument) {
        let result = Math.ceil(argument);
        ++counter;
        return result;
    }
    noInline(opaqueCeilWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let ceil64 = Math.ceil(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueCeilWithException(testObject) !== ceil64)
            throw "Incorrect result in opaqueCeilWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueCeilWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueCeilWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
