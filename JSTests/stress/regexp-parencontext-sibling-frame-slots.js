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

// The YARR JIT's ParenContext save area for a repeating group only holds that group's own
// interior frame slots, not the frame slots of sibling or ancestor-sibling terms. These tests
// have several repeating groups back-to-back (siblings) or nested inside each other
// (ancestor-siblings) so that each group's frame slots land immediately next to another group's,
// exercising backtracking across the boundary between them.

// Sequential top-level repeating groups: (c|d)+ and (e|f)+'s frame slots are (a|b)+'s siblings.
for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/(a|b)+(c|d)+(e|f)+g/.exec("ababccddeeefg"), ["ababccddeeefg", "b", "d", "f"], "sequential groups full match");
    shouldBeArray(/(a|b)+(c|d)+(e|f)+g/.exec("accfg"), ["accfg", "a", "c", "f"], "sequential groups minimal match");
    shouldBe(/(a|b)+(c|d)+(e|f)+g/.exec("aczzfg"), null, "sequential groups forced backtrack failure");

    let re = /(a|b)+(c|d)+(e|f)+g/g;
    let m1 = re.exec("accfg abaccddeg");
    shouldBeArray(m1, ["accfg", "a", "c", "f"], "sequential groups global match 1");
    let m2 = re.exec("accfg abaccddeg");
    shouldBeArray(m2, ["abaccddeg", "a", "d", "e"], "sequential groups global match 2");
}

// Nested repeating groups: the inner (a|b)+ group's frame slots are interior to the outer
// group, while the outer group's own frame slots are ancestor-siblings to any top-level term
// that follows it (here, the "d+e" tail).
for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/((a|b)+c)+d+e/.exec("abcabcddde"), ["abcabcddde", "abc", "b"], "nested groups full match");
    shouldBeArray(/((a|b)+c)+d+e/.exec("acdde"), ["acdde", "ac", "a"], "nested groups minimal match");

    shouldBe(/(p(a|aa)*q)+r(c|cc)*s/.exec("paaqraccs"), null, "nested groups with siblings, no match");
    shouldBeArray(/(p(a|aa)*q)+r(c|cc)*s/.exec("paqpaaqrccs"), ["paqpaaqrccs", "paaq", "a", "c"], "nested groups with siblings, match after backtrack");
}
