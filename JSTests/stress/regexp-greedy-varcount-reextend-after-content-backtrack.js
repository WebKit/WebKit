// Regression test: after popping iterations below quantityMinCount and using
// parenthesesDoBacktrack to find a new content distribution, the interpreter
// must re-run the greedy extension loop up to quantityMaxCount. The new
// distribution leaves the input at a different position, so the iteration
// counts in (min, max] are an unexplored branch — silently skipping them led
// to short matches and corrupted captures.
//
// All cases below force the YARR interpreter (lookbehind / backreference, or
// --useRegExpJIT=0) and should match V8 behavior.

function eq(actual, expected, label) {
    if (actual === expected)
        return;
    if (Array.isArray(actual) && Array.isArray(expected) && actual.length === expected.length) {
        let allEq = true;
        for (let i = 0; i < actual.length; ++i) {
            if (actual[i] !== expected[i]) {
                allEq = false;
                break;
            }
        }
        if (allEq)
            return;
    }
    throw new Error("FAIL " + label + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

function check(re, input, expectedArr, expectedIndex, label) {
    const m = re.exec(input);
    if (expectedArr === null) {
        if (m !== null)
            throw new Error("FAIL " + label + ": got " + JSON.stringify(m) + ", expected null");
        return;
    }
    if (m === null)
        throw new Error("FAIL " + label + ": got null, expected " + JSON.stringify(expectedArr));
    eq(Array.from(m), expectedArr, label + " (groups)");
    eq(m.index, expectedIndex, label + " (index)");
}

// Greedy variable-count parens, anchored, multi-alt forces a content backtrack
// that drops below min (=2). After refilling to min, the third iteration must
// be re-attempted to satisfy `$` at end of input.
check(/(?<=^)(a|aa|b){2,3}$/, "aaba", ["aaba", "a"], 0, "lookbehind aaba");

// Same shape but with a backreference to force the interpreter, exercising the
// capture restoration on backtrack as well.
check(/((\w.)?\2+|c){2,}(.)+(..)/, "cbcbcbbb", ["cbcbcbbb", "c", undefined, "b", "bb"], 0, "backref cbcbcbbb");

// A lookbehind variant that used to return null because the third iteration
// was never attempted after the first iteration backtracked from "aa" to "a".
check(/(?<=^)(aa|a){2,3}$/, "aaa", ["aaa", "a"], 0, "lookbehind aaa");

// Min reachable only after redistributing earlier iterations across alts;
// then max must still be reachable by extending the greedy tail.
check(/(?<=^)(a|aa|b){2,4}b$/, "aaab", ["aaab", "a"], 0, "lookbehind reach max via extend");
