// A quantified group whose body can match the empty string ((?:x*)+, (?:x*)?, (?:x*){2})
// makes the Yarr JIT give up in the middle of matching and fall back to the interpreter.
// Such a pattern must not be inlined into DFG/FTL code: the inlined matcher shares
// registers with the operands of the slow-path call.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

const inputs = ["y", "yx", "yxx", "q", "", "xy", "yxq", "xxx", "yyy", "qy"];

// [regexp, expected result for each input above]
const cases = [
    [/y(?:x*)+/,        "1110011011"],
    [/y(?:x*)*/,        "1110011011"],
    [/y(?:x*)?/,        "1110011011"],
    [/y(?:x*|q)?/,      "1110011011"],
    [/y(?:x*){2}/,      "1110011011"],
    [/y(?:x*){2,}/,     "1110011011"],
    [/^(?:x*)+y/,       "1110011010"],
    [/(?:x*)+q/,        "0001001001"],
    [/y(?:x*)+/u,       "1110011011"],
    [/y(?:x*)+/v,       "1110011011"],
    [/y(?:x*)+$/,       "1110010011"],
    [/(?:x?y?)+z/,      "0000000000"],
];

for (const [re, expected] of cases)
    shouldBe(expected.length, inputs.length, re + " expectation length");

// A RegExp literal inside the function is a NewRegExp node that the strength reduction
// phase can turn into RegExpTestInline.
function makeLiteralTest(re) {
    const test = new Function("s", "return " + re + ".test(s);");
    noInline(test);
    return test;
}

// A RegExp in a global const is folded to a constant cell, so the RegExpObject operand of
// the slow-path call is materialized differently from the literal case.
const globalRegExps = cases.map(([re]) => re);
function makeConstTest(index) {
    const test = new Function("s", "return globalRegExps[" + index + "].test(s);");
    noInline(test);
    return test;
}

for (let c = 0; c < cases.length; ++c) {
    const [re, expected] = cases[c];
    const literalTest = makeLiteralTest(re);
    const constTest = makeConstTest(c);
    for (let i = 0; i < testLoopCount; ++i) {
        const s = inputs[i % inputs.length];
        const e = expected[i % inputs.length] === "1";
        shouldBe(literalTest(s), e, re + ".test(" + JSON.stringify(s) + ") literal");
        shouldBe(constTest(s), e, re + ".test(" + JSON.stringify(s) + ") const");
    }
}
