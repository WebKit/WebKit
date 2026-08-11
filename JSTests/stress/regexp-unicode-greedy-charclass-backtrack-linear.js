function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`FAIL: ${msg}: expected ${expected}, got ${actual}`);
}

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

let np = "\u{10000}"; // non-BMP, 2 code units
let lead = "\uD800";
let trail = "\uDC00";

for (let i = 0; i < testLoopCount; i++) {
    // Backtrack steps back over a BMP character (1 code unit)
    shouldBeArray(/([^"]*)X/u.exec("\u2014aaX"), ["\u2014aaX", "\u2014aa"], "greedy BMP backtrack");

    // Backtrack steps back over a non-BMP character (2 code units)
    shouldBeArray(/([^"]*)a/u.exec("b" + np + np + "a"), ["b" + np + np + "a", "b" + np + np], "greedy non-BMP backtrack");
    shouldBeArray(new RegExp("([^\"]*)" + np, "u").exec("ab" + np), ["ab" + np, "ab"], "backtrack to non-BMP target");

    // Backtrack steps back over lone surrogates (1 code unit each)
    shouldBeArray(/([^"]*)a/u.exec("b" + trail + "a"), ["b" + trail + "a", "b" + trail], "greedy lone trail backtrack");
    shouldBeArray(/([^"]*)a/u.exec("b" + lead + "a"), ["b" + lead + "a", "b" + lead], "greedy lone lead backtrack");
    shouldBeArray(new RegExp("([^\"]*)" + trail + "$", "u").exec("a" + trail + trail), ["a" + trail + trail, "a" + trail], "backtrack to lone trail target");

    // Mixed widths: backtrack repeatedly across pair/lone-surrogate/BMP boundaries
    shouldBeArray(/([^"]*)a/u.exec(np + trail + np + lead + "a"), [np + trail + np + lead + "a", np + trail + np + lead], "greedy mixed-width backtrack");
    shouldBeArray(new RegExp("([^X]*)" + lead + trail + "$", "u").exec("b" + np + np), ["b" + np + np, "b" + np], "backtrack over non-BMP to pair target");

    // Non-greedy sanity
    shouldBeArray(/([^"]*?)(a*)$/u.exec(np + "aaa"), [np + "aaa", np, "aaa"], "non-greedy sanity");

    // Backtrack all the way back to the start of the term (match amount 0)
    shouldBeArray(/b([^"]*)b/u.exec("b" + np + "ab"), ["b" + np + "ab", np + "a"], "backtrack from interior term start");
    shouldBeArray(new RegExp("a([^X]*)" + np + "b", "u").exec("a" + np + "b"), ["a" + np + "b", ""], "backtrack to begin, next term rematches");

    // Greedy with following term failing everywhere: no match
    shouldBe(/[^"]*X/u.test(np + trail + lead + "aaa"), false, "no match after full backtrack");
    shouldBe(new RegExp("[^\"]{0,4}X", "u").test(np + "aaa"), false, "bounded no match");

    // lastIndex / global
    let re = /[^X]*X/gu;
    let s = np + "aX" + trail + "bX";
    shouldBeArray(re.exec(s), [np + "aX"], "global first match");
    shouldBe(re.lastIndex, 4, "global lastIndex after first");
    shouldBeArray(re.exec(s), [trail + "bX"], "global second match");
    shouldBe(re.lastIndex, 7, "global lastIndex after second");
}

// Before the fix, backtracking a greedy variable-width character class rewound to the
// start of the term and rematched forward on every step, making this O(n^3) and taking
// minutes. With the O(1) step-back it completes in milliseconds.
shouldBe(/[^"]*X/u.test("\u2014" + "a".repeat(10000)), false, "complexity canary");
