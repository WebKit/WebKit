function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

function shouldBeArray(actual, expected, msg) {
    if (actual === null && expected !== null)
        throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got null`);
    if (actual === null && expected === null)
        return;
    if (actual.length !== expected.length)
        throw new Error(`FAIL: ${msg}: length mismatch: expected ${expected.length}, got ${actual.length}`);
    for (let i = 0; i < expected.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error(`FAIL: ${msg}: index ${i}: expected ${JSON.stringify(expected[i])}, got ${JSON.stringify(actual[i])}`);
    }
}

// Multi-alternative repeating groups additionally save/restore a returnAddress slot (used to
// resume the forward pass in the right alternative on the next iteration), on top of the
// per-iteration captures and any trailing sibling frame slots. These tests combine that with a
// following capturing group and with backreferences into the repeating group's own captures, so
// a bug in the interior-only save/restore would show up as either the wrong alternative being
// resumed or a stale/incorrect backreference value.

for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/((a)|(bb)|(ccc))+z([0-9])*q/.exec("abbcccz123q"), ["abbcccz123q", "ccc", undefined, undefined, "ccc", "3"], "multi-alt group with trailing sibling group");

    let re = /((a)|(bb)|(ccc))+z([0-9])*q/g;
    shouldBeArray(re.exec("az1q bbz2q"), ["az1q", "a", "a", undefined, undefined, "1"], "multi-alt group, global match 1");
    shouldBeArray(re.exec("az1q bbz2q"), ["bbz2q", "bb", undefined, "bb", undefined, "2"], "multi-alt group, global match 2");

    shouldBeArray(/(a|b)+(x|y)\1/.exec("ababxb"), ["ababxb", "b", "x"], "repeating group capture read back by backreference");
    shouldBe(/(a|b)+(x|y)\1/.exec("ababya"), null, "backreference forces failure when last iteration's capture doesn't match");

    shouldBeArray(/((a)|(b))+\2\3?/.exec("aabbbb"), ["aabbbb", "b", undefined, "b"], "backreferences to duplicate-alternative captures across iterations");
}
