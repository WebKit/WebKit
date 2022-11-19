//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0")
//@ defaultNoEagerRun
"use strict";

let validInputTestCases = [
    // input as string, expected result as string.
    ["undefined", "32"],
    ["null", "32"],
    ["0", "32"],
    ["-0.", "32"],
    ["4", "29"],
    ["Math.PI", "30"],
    ["Infinity", "32"],
    ["-Infinity", "32"],
    ["NaN", "32"],
    ["\"WebKit\"", "32"],
    ["\"4\"", "29"],
    ["{ valueOf: () => { return 4; } }", "29"],
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


// Test Math.clz32() without arguments.
function opaqueClz32NoArgument() {
    return Math.clz32();
}
noInline(opaqueClz32NoArgument);
noOSRExitFuzzing(opaqueClz32NoArgument);

function testNoArgument() {
    for (let i = 0; i < 1e4; ++i) {
        let output = opaqueClz32NoArgument();
        if (output !== 32) {
            throw "Failed opaqueClz32NoArgument";
        }
    }
    if (numberOfDFGCompiles(opaqueClz32NoArgument) > 1)
        throw "The call without arguments should never exit.";
}
testNoArgument();


// Test Math.clz32() with a very polymorphic input. All test cases are seen at each iteration.
function opaqueAllTypesClz32(argument) {
    return Math.clz32(argument);
}
noInline(opaqueAllTypesClz32);
noOSRExitFuzzing(opaqueAllTypesClz32);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let testCaseInput of validInputTypedTestCases) {
            let output = opaqueAllTypesClz32(testCaseInput[0]);
            if (!isIdentical(output, testCaseInput[1]))
                throw "Failed testAllTypesCall for input " + testCaseInput[0] + " expected " + testCaseInput[1] + " got " + output;
        }
    }
    if (numberOfDFGCompiles(opaqueAllTypesClz32) > 2)
        throw "We should have detected clz32() was polymorphic and generated a generic version.";
}
testAllTypesCall();


// Test Math.clz32() on a completely typed input. Every call see only one type.
function testSingleTypeCall() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueClz32(argument) {
                return Math.clz32(argument);
            }
            noInline(opaqueClz32);
            noOSRExitFuzzing(opaqueClz32);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueClz32(${testCaseInput[0]}), ${testCaseInput[1]})) {
                    throw "Failed testSingleTypeCall()";
                }
            }
            if (numberOfDFGCompiles(opaqueClz32) > 1)
                throw "We should have compiled a single clz32 for the expected type.";
        `);
    }
}
testSingleTypeCall();


// Test Math.clz32() on constants
function testConstant() {
    for (let testCaseInput of validInputTestCases) {
        eval(`
            function opaqueClz32OnConstant() {
                return Math.clz32(${testCaseInput[0]});
            }
            noInline(opaqueClz32OnConstant);
            noOSRExitFuzzing(opaqueClz32OnConstant);

            for (let i = 0; i < 1e4; ++i) {
                if (!isIdentical(opaqueClz32OnConstant(), ${testCaseInput[1]})) {
                    throw "Failed testConstant()";
                }
            }
            if (numberOfDFGCompiles(opaqueClz32OnConstant) > 1)
                throw "We should have compiled a single clz32 for the expected type.";
        `);
    }
}
testConstant();


// Verify we call valueOf() exactly once per call.
function opaqueClz32ForSideEffects(argument) {
    return Math.clz32(argument);
}
noInline(opaqueClz32ForSideEffects);
noOSRExitFuzzing(opaqueClz32ForSideEffects);

function testSideEffect() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let clz3216 = Math.clz32(16);
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueClz32ForSideEffects(testObject) !== clz3216)
            throw "Incorrect result in testSideEffect()";
    }
    if (testObject.counter !== 1e4)
        throw "Failed testSideEffect()";
    if (numberOfDFGCompiles(opaqueClz32ForSideEffects) > 1)
        throw "opaqueClz32ForSideEffects() is predictable, it should only be compiled once.";
}
testSideEffect();


// Verify clz32() is not subject to CSE if the argument has side effects.
function opaqueClz32ForCSE(argument) {
    return Math.clz32(argument) + Math.clz32(argument) + Math.clz32(argument);
}
noInline(opaqueClz32ForCSE);
noOSRExitFuzzing(opaqueClz32ForCSE);

function testCSE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    let clz3216 = Math.clz32(16);
    let threeClz3216 = clz3216 + clz3216 + clz3216;
    for (let i = 0; i < 1e4; ++i) {
        if (opaqueClz32ForCSE(testObject) !== threeClz3216)
            throw "Incorrect result in testCSE()";
    }
    if (testObject.counter !== 3e4)
        throw "Failed testCSE()";
    if (numberOfDFGCompiles(opaqueClz32ForCSE) > 1)
        throw "opaqueClz32ForCSE() is predictable, it should only be compiled once.";
}
testCSE();


// Verify clz32() is not subject to DCE if the argument has side effects.
function opaqueClz32ForDCE(argument) {
    Math.clz32(argument);
}
noInline(opaqueClz32ForDCE);
noOSRExitFuzzing(opaqueClz32ForDCE);

function testDCE() {
    let testObject = {
        counter: 0,
        valueOf: function() { ++this.counter; return 16; }
    };
    for (let i = 0; i < 1e4; ++i) {
        opaqueClz32ForDCE(testObject);
    }
    if (testObject.counter !== 1e4)
        throw "Failed testDCE()";
    if (numberOfDFGCompiles(opaqueClz32ForDCE) > 1)
        throw "opaqueClz32ForDCE() is predictable, it should only be compiled once.";
}
testDCE();


// Test exceptions in the argument.
function testException() {
    let counter = 0;
    function opaqueClz32WithException(argument) {
        let result = Math.clz32(argument);
        ++counter;
        return result;
    }
    noInline(opaqueClz32WithException);

    let testObject = { valueOf: () => {  return 64; } };
    let clz3264 = Math.clz32(64);

    // Warm up without exception.
    for (let i = 0; i < 1e3; ++i) {
        if (opaqueClz32WithException(testObject) !== clz3264)
            throw "Incorrect result in opaqueClz32WithException()";
    }

    let testThrowObject = { valueOf: () => { throw testObject; return 64; } };

    for (let i = 0; i < 1e2; ++i) {
        try {
            if (opaqueClz32WithException(testThrowObject) !== 8)
                throw "This code should not be reached!!";
        } catch (e) {
            if (e !== testObject) {
                throw "Wrong object thrown from opaqueClz32WithException."
            }
        }
    }

    if (counter !== 1e3) {
        throw "Invalid count in testException()";
    }
}
testException();
