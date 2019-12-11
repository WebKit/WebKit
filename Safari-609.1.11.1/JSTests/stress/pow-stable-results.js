// This test verify the results of `**` do not change as we change optimization tier.

function opaquePow(a, b)
{
    return a ** b;
}
noInline(opaquePow);


let caseStrings = [
    "0",
    "-0.",
    "0.5",
    "1",
    "2",
    "-0.5",
    "-1",
    "999",
    "1000",
    "1001",
    "NaN",
    "Infinity",
    "-Infinity",
    "Math.PI",
    "Math.LN2",
    "Math.LN10",
    "Math.E",
    "Math.LOG10E",
    "Math.LOG2E",
    "Math.SQRT2"
];
let cases = [];
for (let caseString of caseStrings) {
    cases.push(eval(caseString));
}

let expectedResults = [];
let constantBaseFunctions = [];
let constantExponentFunctions = [];
for (let i = 0; i < cases.length; ++i) {
    let base = cases[i];

    expectedResults[i] = [];
    for (let j = 0; j < cases.length; ++j) {
        let exponent = cases[j];
        expectedResults[i][j] = base ** exponent;
    }

    eval("constantBaseFunctions[i] = function (exponent) { return (" + caseStrings[i] + ") ** exponent; }");
    eval("constantExponentFunctions[i] = function (base) { return base ** (" + caseStrings[i] + "); }");
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

for (let tierUpLoopCounter = 0; tierUpLoopCounter < 1e3; ++tierUpLoopCounter) {
    for (let i = 0; i < cases.length; ++i) {
        let base = cases[i];
        for (let j = 0; j < cases.length; ++j) {
            let exponent = cases[j];
            let expectedResult = expectedResults[i][j];
            let result = opaquePow(base, exponent);
            if (!isIdentical(result, expectedResult))
                throw `Failed opaquePow with base = ${base} exponent = ${exponent} expected (${expectedResult}) got (${result})`;

            result = constantBaseFunctions[i](exponent);
            if (!isIdentical(result, expectedResult))
                throw `Failed constantBaseFunctions with base = ${base} exponent = ${exponent} expected (${expectedResult}) got (${result})`;

            result = constantExponentFunctions[j](base);
            if (!isIdentical(result, expectedResult))
                throw `Failed constantExponentFunctions with base = ${base} exponent = ${exponent} expected (${expectedResult}) got (${result})`;
        }
    }
}
