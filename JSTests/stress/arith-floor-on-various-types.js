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
    ["-0.5", "-1"],
    ["4", "4"],
    ["42.1", "42"],
    ["42.5", "42"],
    ["42.9", "42"],
    ["-42.1", "-43"],
    ["-42.5", "-43"],
    ["-42.9", "-43"],
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
    ["{ valueOf: () => { return -0.5; } }", "-1"],
    ["{ valueOf: () => { return Number.MIN_SAFE_INTEGER; } }", "-9007199254740991"],
    ["{ valueOf: () => { return Number.MAX_SAFE_INTEGER; } }", "9007199254740991"],
    ["{ valueOf: () => { return 0x80000000|0; } }", "-2147483648"],
    ["{ valueOf: () => { return 0x7fffffff|0; } }", "2147483647"],
    ["{ valueOf: () => { return (0x80000000|0) - 0.5; } }", "-2147483649"],
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


// Test Math.floor() without arguments.
function opaqueFloorNoArgument() {
    return Math.floor();
}
noInline(opaqueFloorNoArgument);
noOSRExitFuzzing(opaqueFloorNoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueFloorNoArgument();
        if (!isIdentical(output, NaN)) {
            throw "Failed opaqueFloorNoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueFloorNoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.floor() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesFloor(argument) {
    return Math.floor(argument);
}
noInline(opaqueAllTypesFloor);
noOSRExitFuzzing(opaqueAllTypesFloor);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesFloor(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesFloor) > 3)
        throw "We should have detected floor() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Polymorphic input but negative zero is not observable.
function opaqueAllTypesFloorWithoutNegativeZero(argument) {
    return Math.floor(argument) + 0;
}
noInline(opaqueAllTypesFloorWithoutNegativeZero);
noOSRExitFuzzing(opaqueAllTypesFloorWithoutNegativeZero);

function testAllTypesWithoutNegativeZeroCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesFloorWithoutNegativeZero(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1] + 0))
                throw "Failed testAllTypesWithoutNegativeZeroCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesFloor) > 3)
        throw "We should have detected floor() was polymorphic and generated a generic version.";
}
testAllTypesWithoutNegativeZeroCall();


// Test Math.floor() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueFloor(argument) {
                return Math.floor(argument);
            }
            noInline(opaqueFloor);
            noOSRExitFuzzing(opaqueFloor);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueFloor(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueFloor) > 1)
                throw "We should have compiled a single floor for the expected type.";
        `);
    }
}
testSingleTypeCall();


function checkCompileCountForUselessNegativeZero(testFunction)
{
    return numberOfDFGCompiles(testFunction) <= 1;
}


// Test Math.floor() on a completely typed input, but without negative zero.
function testSingleTypeWithoutNegativeZeroCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueFloor(argument) {
                return Math.floor(argument) + 0;
            }
            noInline(opaqueFloor);
            noOSRExitFuzzing(opaqueFloor);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueFloor(${testCaseInput[0]}), ${testCaseInput[1]} + 0)) {
                    throw "Failed testSingleTypeWithoutNegativeZeroCall()";
                }
            }
            if (!checkCompileCountForUselessNegativeZero(opaqueFloor))
                throw "We should have compiled a single floor for the expected type.";
        `);
    }
}
testSingleTypeWithoutNegativeZeroCall();


// Test Math.floor() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueFloorOnConstant() {
                return Math.floor(${testCaseInput[0]});
            }
            noInline(opaqueFloorOnConstant);
            noOSRExitFuzzing(opaqueFloorOnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueFloorOnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueFloorOnConstant) > 1)
                throw "We should have compiled a single floor for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueFloorForSideEffects(argument) {
    return Math.floor(argument);
}
noInline(opaqueFloorForSideEffects);
noOSRExitFuzzing(opaqueFloorForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let floor16 = Math.floor(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueFloorForSideEffects(testObject) !== floor16)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueFloorForSideEffects) > 1)
        throw "opaqueFloorForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify floor() is not subject to CSE if the argument has side effects.
function opaqueFloorForCSE(argument) {
    return Math.floor(argument) + Math.floor(argument) + Math.floor(argument);
}
noInline(opaqueFloorForCSE);
noOSRExitFuzzing(opaqueFloorForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let floor16 = Math.floor(16);
    let threeFloor16 = floor16 + floor16 + floor16;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueFloorForCSE(testObject) !== threeFloor16)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueFloorForCSE) > 1)
        throw "opaqueFloorForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify floor() is not subject to DCE if the argument has side effects.
function opaqueFloorForDCE(argument) {
    Math.floor(argument);
}
noInline(opaqueFloorForDCE);
noOSRExitFuzzing(opaqueFloorForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueFloorForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueFloorForDCE) > 1)
        throw "opaqueFloorForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueFloorWithException(argument) {
        let result = Math.floor(argument);
        ++counter;
        return result;
    }
    noInline(opaqueFloorWithException);

    let testObject = { valueOf: () => {  return 64; } };
    let floor64 = Math.floor(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueFloorWithException(testObject) !== floor64)
            throw "Incorrect result in opaqueFloorWithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueFloorWithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueFloorWithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
