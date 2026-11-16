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

// The YARR JIT copies a repeating group's interior frame slots to/from its ParenContext either by
// direct unrolled copies (fewer than 5 slots) or by a runtime copy loop indexed relative to the
// group's own frame base (5 or more slots, to bound generated code size). These tests force a
// group with >=5 interior slots to exercise the loop path, and force several frame-slot-owning
// terms *before* the group (a large frame base) to exercise the base-relative indexing into the
// (now much smaller) ParenContext save area.

// Large interior: six variable-quantified character classes inside one repeating group.
for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/([ab]?[cd]?[ef]?[gh]?[ij]?[kl]?)+z/.exec("acegikacegikz"), ["acegikacegikz", "acegik"], "large interior, multiple iterations");
    shouldBeArray(/([ab]?[cd]?[ef]?[gh]?[ij]?[kl]?)+z/.exec("z"), ["z", ""], "large interior, single empty iteration");
}

// Large base: six preceding capturing groups (each owning its own frame slot) before the
// repeating group, so the group's own interior sits far from the start of the frame.
for (let i = 0; i < testLoopCount; i++) {
    shouldBeArray(/(z*)(y*)(x*)(w*)(v*)(u*)(a|ab)+c/.exec("zyxwvuababac"), ["zyxwvuababac", "z", "y", "x", "w", "v", "u", "a"], "large base, all preceding groups match");
    shouldBeArray(/(z*)(y*)(x*)(w*)(v*)(u*)(a|ab)+c/.exec("ac"), ["ac", "", "", "", "", "", "", "a"], "large base, all preceding groups empty");
}
