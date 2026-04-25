function shouldBeArray(actual, expected, msg) {
    if (actual === null || expected === null) {
        if (actual !== expected)
            throw new Error(`FAIL: ${msg}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
        return;
    }
    if (actual.length !== expected.length)
        throw new Error(`FAIL: ${msg}: length mismatch: expected ${expected.length}, got ${actual.length}`);
    for (let i = 0; i < expected.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error(`FAIL: ${msg}: index ${i}: expected ${JSON.stringify(expected[i])}, got ${JSON.stringify(actual[i])}`);
    }
}

let np = "\u{10000}"; // U+10000 (non-BMP, 2 code units)
let emoji = "\u{1F600}";

for (let i = 0; i < testLoopCount; i++) {
    // Backtrack from in-loop failure: index advanced for surrogates, then class fails
    shouldBeArray(/[a\u{10000}]{2}Z|./u.exec(np + np + "X"), [np], "{2} 2x non-BMP, fail at Z, alt2 dot");
    shouldBeArray(/[a\u{10000}]{3}Z|b/u.exec("a" + np + "Xb"), ["b"], "{3} BMP+non-BMP, fail in loop, alt2 char");
    shouldBeArray(/[a\u{10000}]{2}Z|\u{10000}a/u.exec(np + "aXY"), [np + "a"], "{2} non-BMP+BMP, fail at Z, alt2 seq");
    shouldBeArray(/[a\u{10000}]{3}|./u.exec(np + "aXY"), [np], "{3} non-BMP+BMP, fail in loop (count 2/3), alt2 dot");
    shouldBeArray(/[a\u{10000}]{2}Z|X/u.exec("aaX"), ["X"], "{2} all-BMP, fail at Z, alt2 (sanity: index unchanged)");
    shouldBeArray(/[a\u{10000}]{2}Z|\u{10000}\u{10000}/u.exec(np + np + "X"), [np + np], "{2} 2x non-BMP, fail at Z, alt2 same");

    // Backtrack from following-term failure: full {n} matched, index advanced, then trailing fails
    shouldBeArray(/[a\u{10000}]{2}Z|a\u{10000}/u.exec("a" + np + "X"), ["a" + np], "{2} matched, Z fails, alt2 prefix");
    shouldBeArray(/[a\u{10000}]{3}Q|\u{10000}{2}/u.exec(np + np + np + "X"), [np + np], "{3} 3x non-BMP matched, Q fails, alt2");

    // Inverted class (also takes mixed-width else branch)
    shouldBeArray(/[^b]{2}Z|./u.exec(np + np + "X"), [np], "{2} inverted 2x non-BMP, fail at Z, alt2 dot");
    shouldBeArray(/[^b]{2}Z|\u{10000}a/u.exec(np + "aX"), [np + "a"], "{2} inverted non-BMP+BMP, fail at Z, alt2");

    // /v flag
    shouldBeArray(/[a\u{10000}]{2}Z|./v.exec(np + np + "X"), [np], "{2} 2x non-BMP /v, fail at Z, alt2 dot");

    // Multiple surrogates in {n} (cumulative index drift)
    shouldBeArray(/[a\u{10000}]{4}Z|./u.exec(np + np + np + np + "X"), [np], "{4} 4x non-BMP (drift+4), fail at Z, alt2 dot");
    shouldBeArray(/[a\u{10000}]{4}Z|\u{10000}{4}/u.exec(np + np + np + np + "X"), [np + np + np + np], "{4} 4x non-BMP, fail, alt2 4x");

    // Mixed BMP and non-BMP in same {n} (partial drift)
    shouldBeArray(/[a\u{10000}]{4}Z|./u.exec("a" + np + "a" + np + "X"), ["a"], "{4} BMP-nonBMP-BMP-nonBMP (drift+2), fail, alt2 dot");

    // Short input: jumpIfNoAvailableInput fires before loop (storeToFrame must precede jump)
    shouldBeArray(/[a\u{10000}]{2}Z|x/u.exec(""), null, "{2} empty input (jump fires before loop)");
    shouldBeArray(/[a\u{10000}]{2}Z|x/u.exec("x"), ["x"], "{2} 1-unit input (jump fires before loop)");

    // Capture group around the term
    shouldBeArray(/([a\u{10000}]{2})Z|(.)/u.exec(np + np + "X"), [np, undefined, np], "{2} capture, fail at Z, alt2 dot");

    // Positive cases: should still match
    shouldBeArray(/[a\u{10000}]{2}Z/u.exec(np + np + "Z"), [np + np + "Z"], "{2} 2x non-BMP + Z (positive)");
    shouldBeArray(/[a\u{10000}]{3}/u.exec("a" + np + "a"), ["a" + np + "a"], "{3} mixed (positive)");
    shouldBeArray(/[a\u{10000}]{2}/u.exec(np + "a"), [np + "a"], "{2} non-BMP+BMP (positive)");
    shouldBeArray(/[^b]{3}Z/u.exec(np + np + np + "Z"), [np + np + np + "Z"], "{3} inverted 3x non-BMP + Z (positive)");

    // Fixed-width class control: hasOnlyNonBMP, no index drift, should still work
    shouldBeArray(/[\u{1F600}-\u{1F64F}]{2}Z|./u.exec(emoji + emoji + "X"), [emoji], "{2} fixed-width non-BMP-only, fail at Z, alt2 (control)");
    shouldBeArray(/[\u{1F600}-\u{1F64F}]{2}Z/u.exec(emoji + emoji + "Z"), [emoji + emoji + "Z"], "{2} fixed-width non-BMP-only + Z (positive control)");
}
