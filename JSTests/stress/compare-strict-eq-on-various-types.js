//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ defaultNoEagerRun
"use strict";

function opaqueKitString() {
    return "Kit";
}
noInline(opaqueKitString);

let someObject = new Object;
let validInputTestCases = [
    "undefined",
    "Symbol(\"WebKit\")",
    "null",
    "true",
    "false",
    "0",
    "{ valueOf: () => { return Math.E; } }",
    "-0",
    "42",
    "-42",
    "Math.PI",
    "NaN",
    "\"WebKit\"",
    "\"Web\" + opaqueKitString()",
    "new String(\"WebKit\")",
    "someObject",
    "validInputTestCases",
];

let leftCases = validInputTestCases.map((element) => { return eval("(" + element + ")"); });
let rightCases = validInputTestCases.map((element) => { return eval("(" + element + ")"); });

// Baseline results.
let expectedEqualResults = [];
let expectedNotEqualResults = [];
for (let testCaseInputLeft of leftCases) {
    let equalResultRow = [];
    let notEqualResultRow = [];
    for (let testCaseInputRight of rightCases) {
        equalResultRow.push(testCaseInputLeft === testCaseInputRight);
        notEqualResultRow.push(testCaseInputLeft !== testCaseInputRight);
    }
    expectedEqualResults.push(equalResultRow);
    expectedNotEqualResults.push(notEqualResultRow);
}

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


// Very polymorphic compare.
function opaqueStrictEqualAllTypes(left, right) {
    return left === right;
}
noInline(opaqueStrictEqualAllTypes);
noOSRExitFuzzing(opaqueStrictEqualAllTypes);

function opaqueStrictNotEqualAllTypes(left, right) {
    return left !== right;
}
noInline(opaqueStrictNotEqualAllTypes);
noOSRExitFuzzing(opaqueStrictNotEqualAllTypes);

function testAllTypesCall() {
    for (let i = 0; i < 1e3; ++i) {
        for (let leftCaseIndex = 0; leftCaseIndex < leftCases.length; ++leftCaseIndex) {
            for (let rightCaseIndex = 0; rightCaseIndex < rightCases.length; ++rightCaseIndex) {
                let strictEqualOutput = opaqueStrictEqualAllTypes(leftCases[leftCaseIndex], rightCases[rightCaseIndex]);
                if (!isIdentical(strictEqualOutput, expectedEqualResults[leftCaseIndex][rightCaseIndex])) {
                    throw "Failed testAllTypesCall for equality. Left = " +
                        leftCases[leftCaseIndex] +
                        ", Right = " +
                        rightCases[rightCaseIndex] +
                        ", Result = " +
                        strictEqualOutput +
                        " (case: " + leftCaseIndex + ", " + rightCaseIndex + ")";
                }

                let strictNotEqualOutput = opaqueStrictNotEqualAllTypes(leftCases[leftCaseIndex], rightCases[rightCaseIndex]);
                if (!isIdentical(strictNotEqualOutput, expectedNotEqualResults[leftCaseIndex][rightCaseIndex])) {
                    throw "Failed testAllTypesCall for !equality. Left = " +
                        leftCases[leftCaseIndex] +
                        ", Right = " +
                        rightCases[rightCaseIndex] +
                        ", Result = " +
                        strictEqualOutput +
                        " (case: " + leftCaseIndex + ", " + rightCaseIndex + ")";
                }
            }
        }
    }
    if (numberOfDFGCompiles(opaqueStrictEqualAllTypes) > 5)
        throw "opaqueStrictEqualAllTypes() should have been quickly compiled as fully polymorphic.";
    if (opaqueStrictNotEqualAllTypes(opaqueStrictEqualAllTypes) > 5)
        throw "opaqueStrictEqualAllTypes() should have been quickly compiled as fully polymorphic.";
}
testAllTypesCall();


// Comparing String to every type.
function opaqueStrictEqualStringToAllTypes(left, right) {
    return left === right;
}
noInline(opaqueStrictEqualStringToAllTypes);
noOSRExitFuzzing(opaqueStrictEqualStringToAllTypes);

function opaqueStrictEqualAllTypesToString(left, right) {
    return left === right;
}
noInline(opaqueStrictEqualAllTypesToString);
noOSRExitFuzzing(opaqueStrictEqualAllTypesToString);

function opaqueStrictNotEqualStringToAllTypes(left, right) {
    return left !== right;
}
noInline(opaqueStrictNotEqualStringToAllTypes);
noOSRExitFuzzing(opaqueStrictNotEqualStringToAllTypes);

function opaqueStrictNotEqualAllTypesToString(left, right) {
    return left !== right;
}
noInline(opaqueStrictNotEqualAllTypesToString);
noOSRExitFuzzing(opaqueStrictNotEqualAllTypesToString);

function testStringToAllCompare() {
    const leftStringIndex = leftCases.indexOf("WebKit");
    for (let i = 0; i < 1e3; ++i) {
        for (let rightCaseIndex = 0; rightCaseIndex < rightCases.length; ++rightCaseIndex) {
            let rightCase = rightCases[rightCaseIndex];
            let strictEqualOutput = opaqueStrictEqualStringToAllTypes("Web" + opaqueKitString(), rightCase);
            if (!isIdentical(strictEqualOutput, expectedEqualResults[leftStringIndex][rightCaseIndex])) {
                throw "Failed opaqueStrictEqualStringToAllTypes() with right = " + rightCase;
            }
            let strictNotEqualOutput = opaqueStrictNotEqualStringToAllTypes("Web" + opaqueKitString(), rightCase);
            if (!isIdentical(strictNotEqualOutput, expectedNotEqualResults[leftStringIndex][rightCaseIndex])) {
                throw "Failed opaqueStrictNotEqualStringToAllTypes() with right = " + rightCase;
            }
        }
    }

    const rightStringIndex = leftCases.lastIndexOf("WebKit");
    for (let i = 0; i < 1e3; ++i) {
        for (let leftCaseIndex = 0; leftCaseIndex < leftCases.length; ++leftCaseIndex) {
            let leftCase = leftCases[leftCaseIndex];
            let strictEqualOutput = opaqueStrictEqualAllTypesToString(leftCase, "Web" + opaqueKitString());
            if (!isIdentical(strictEqualOutput, expectedEqualResults[leftCaseIndex][rightStringIndex])) {
                throw "Failed opaqueStrictEqualAllTypesToString() with left = " + leftCase;
            }
            let strictNotEqualOutput = opaqueStrictNotEqualAllTypesToString(leftCase, "Web" + opaqueKitString());
            if (!isIdentical(strictNotEqualOutput, expectedNotEqualResults[leftCaseIndex][rightStringIndex])) {
                throw "Failed opaqueStrictNotEqualAllTypesToString() with left = " + leftCase;
            }
        }
    }

    if (numberOfDFGCompiles(opaqueStrictEqualStringToAllTypes) > 2)
        throw "opaqueStrictEqualStringToAllTypes() should quickly converge its types.";
    if (numberOfDFGCompiles(opaqueStrictEqualAllTypesToString) > 2)
        throw "opaqueStrictEqualAllTypesToString() should quickly converge its types.";
    if (numberOfDFGCompiles(opaqueStrictNotEqualStringToAllTypes) > 2)
        throw "opaqueStrictNotEqualStringToAllTypes() should quickly converge its types.";
    if (numberOfDFGCompiles(opaqueStrictNotEqualAllTypesToString) > 2)
        throw "opaqueStrictNotEqualAllTypesToString() should quickly converge its types.";
}
testStringToAllCompare();


// Compare one type to all the others.
function compareOneTypeToAll() {
    for (let leftCaseIndex = 0; leftCaseIndex < validInputTestCases.length; ++leftCaseIndex) {
        let leftCase = validInputTestCases[leftCaseIndex];
        eval(`
            function opaqueStrictEqualOneTypeToAll(left, right) {
                return left === right;
            }
            noInline(opaqueStrictEqualOneTypeToAll);
            noOSRExitFuzzing(opaqueStrictEqualOneTypeToAll);

            function opaqueStrictNotEqualOneTypeToAll(left, right) {
                return left !== right;
            }
            noInline(opaqueStrictNotEqualOneTypeToAll);
            noOSRExitFuzzing(opaqueStrictNotEqualOneTypeToAll);

            for (let i = 0; i < 1e3; ++i) {
                for (let rightCaseIndex = 0; rightCaseIndex < rightCases.length; ++rightCaseIndex) {
                    let strictEqualOutput = opaqueStrictEqualOneTypeToAll(${leftCase}, rightCases[rightCaseIndex]);
                    if (!isIdentical(strictEqualOutput, expectedEqualResults[${leftCaseIndex}][rightCaseIndex])) {
                        throw "Failed opaqueStrictEqualOneTypeToAll() with left case = " + ${leftCase} + ", right case = " + rightCases[rightCaseIndex];
                    }
                    let strictNotEqualOutput = opaqueStrictNotEqualOneTypeToAll(${leftCase}, rightCases[rightCaseIndex]);
                    if (!isIdentical(strictNotEqualOutput, expectedNotEqualResults[${leftCaseIndex}][rightCaseIndex])) {
                        throw "Failed opaqueStrictNotEqualOneTypeToAll() with left case = " + ${leftCase} + ", right case = " + rightCases[rightCaseIndex];
                    }
                }
            }
             `);
    }
}
compareOneTypeToAll();

function compareAllTypesToOne() {
    for (let rightCaseIndex = 0; rightCaseIndex < validInputTestCases.length; ++rightCaseIndex) {
        let rightCase = validInputTestCases[rightCaseIndex];
        eval(`
            function opaqueStrictEqualAllToOne(left, right) {
                return left === right;
            }
            noInline(opaqueStrictEqualAllToOne);
            noOSRExitFuzzing(opaqueStrictEqualAllToOne);

            function opaqueStrictNotEqualAllToOne(left, right) {
                return left !== right;
            }
            noInline(opaqueStrictNotEqualAllToOne);
            noOSRExitFuzzing(opaqueStrictNotEqualAllToOne);

            for (let i = 0; i < 1e3; ++i) {
                for (let leftCaseIndex = 0; leftCaseIndex < leftCases.length; ++leftCaseIndex) {
                    let strictEqualOutput = opaqueStrictEqualAllToOne(leftCases[leftCaseIndex], ${rightCase});
                    if (!isIdentical(strictEqualOutput, expectedEqualResults[leftCaseIndex][${rightCaseIndex}])) {
                        throw "Failed opaqueStrictEqualAllToOne() with left case = " + leftCases[leftCaseIndex] + ", right case = " + ${rightCase};
                    }
                    let strictNotEqualOutput = opaqueStrictNotEqualAllToOne(leftCases[leftCaseIndex], ${rightCase});
                    if (!isIdentical(strictNotEqualOutput, expectedNotEqualResults[leftCaseIndex][${rightCaseIndex}])) {
                        throw "Failed opaqueStrictNotEqualAllToOne() with left case = " + leftCases[leftCaseIndex] + ", right case = " + ${rightCase};
                    }
                }
            }
             `);
    }
}
compareAllTypesToOne();
