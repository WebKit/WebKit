// Verify the sticky RegExp.test first-character fast-fail filter: a filtered no-match must behave
// exactly like the operation - reset lastIndex to 0, return false, and leave RegExp's global match
// state (RegExp.$1, lastMatch, input, contexts) untouched.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrow(fn, name) {
    let threw = false;
    try { fn(); } catch (e) { threw = true; shouldBe(e.constructor.name, name); }
    if (!threw)
        throw new Error("expected " + name);
}

// A capturing sticky regexp that cannot match empty: first char must be a digit.
const re = /(\d)(\d)?/y;
const input = "a12b3";

function step() {
    // Prior success at index 1 ("12"), which records global state and advances lastIndex.
    re.lastIndex = 1;
    shouldBe(re.test(input), true);
    shouldBe(re.lastIndex, 3);
    const savedLastMatch = RegExp.lastMatch;   // "12"
    const savedParen1 = RegExp.$1;              // "1"
    const savedParen2 = RegExp.$2;              // "2"
    const savedLeft = RegExp.leftContext;       // "a"
    const savedRight = RegExp.rightContext;     // "b3"
    const savedInput = RegExp.input;            // input
    shouldBe(savedLastMatch, "12");
    shouldBe(savedParen1, "1");
    shouldBe(savedParen2, "2");

    // No-match at index 0 ('a' is not a digit): this is the filtered fast-fail path.
    re.lastIndex = 0;
    shouldBe(re.test(input), false);
    shouldBe(re.lastIndex, 0);                  // sticky no-match resets lastIndex to 0.
    // Global state must be UNCHANGED by the failed test.
    shouldBe(RegExp.lastMatch, savedLastMatch);
    shouldBe(RegExp.$1, savedParen1);
    shouldBe(RegExp.$2, savedParen2);
    shouldBe(RegExp.leftContext, savedLeft);
    shouldBe(RegExp.rightContext, savedRight);
    shouldBe(RegExp.input, savedInput);

    // No-match at a filtered position past the last digit ('b' at index 3).
    re.lastIndex = 3;
    shouldBe(re.test(input), false);
    shouldBe(re.lastIndex, 0);

    // lastIndex beyond the end: false + reset, matching operationRegExpTestString.
    re.lastIndex = 100;
    shouldBe(re.test(input), false);
    shouldBe(re.lastIndex, 0);

    // A match at a byte that passes the filter (digit "3" at index 4) advances lastIndex.
    re.lastIndex = 4;
    shouldBe(re.test(input), true);
    shouldBe(re.lastIndex, 5);
}

for (let i = 0; i < testLoopCount; ++i)
    step();

// Inverted leading class and quantified leading group are now filterable for sticky test too.
(function () {
    const reInv = /[^a-z]/y;
    const reGrp = /([0-9a-fA-F]){4}/y;
    for (let i = 0; i < testLoopCount; ++i) {
        reInv.lastIndex = 0;
        shouldBe(reInv.test("Abc"), true);   // 'A' not in a-z
        shouldBe(reInv.lastIndex, 1);
        reInv.lastIndex = 0;
        shouldBe(reInv.test("abc"), false);  // 'a' filtered
        shouldBe(reInv.lastIndex, 0);

        reGrp.lastIndex = 0;
        shouldBe(reGrp.test("dead"), true);
        shouldBe(reGrp.lastIndex, 4);
        reGrp.lastIndex = 0;
        shouldBe(reGrp.test("zzzz"), false); // 'z' filtered
        shouldBe(reGrp.lastIndex, 0);
    }
})();

// Negative lastIndex is not isUInt32; the filter must delegate to the operation, which clamps and
// searches from 0. Here index 0 is 'a' (no match) so the result is false with lastIndex reset.
(function () {
    const re2 = /(\d)(\d)?/y;
    for (let i = 0; i < testLoopCount; ++i) {
        re2.lastIndex = -5;
        shouldBe(re2.test("a12"), false);
        shouldBe(re2.lastIndex, 0);
    }
})();

// A non-writable lastIndex must make a failing sticky test throw a TypeError (Set(...,,true)); the
// filter guards on the writable flag and delegates to the operation, which throws.
(function () {
    for (let i = 0; i < testLoopCount; ++i) {
        const re3 = /\d+/y;
        re3.lastIndex = 0;
        Object.defineProperty(re3, "lastIndex", { writable: false });
        shouldThrow(() => re3.test("abc"), "TypeError");
    }
})();

// 16-bit (non-Latin-1) subject strings must still behave correctly (filter is 8-bit only, so this
// exercises the delegated path). The digit "5" is Latin-1 but the string is 16-bit.
(function () {
    const re4 = /\d/y;
    const wide = "\u3042\u3044" + "5";   // forces a 16-bit string
    for (let i = 0; i < testLoopCount; ++i) {
        re4.lastIndex = 0;
        shouldBe(re4.test(wide), false);   // first char is a wide non-digit
        shouldBe(re4.lastIndex, 0);
        re4.lastIndex = 2;
        shouldBe(re4.test(wide), true);
        shouldBe(re4.lastIndex, 3);
    }
})();

// A .compile() that changes the pattern fires the realm's RegExp-recompiled watchpoint, jettisoning
// the code that baked the old bitmap: the old filter must not produce wrong answers for the new
// pattern.
(function () {
    const re5 = /a+/y;
    function check(s, expected) { re5.lastIndex = 0; shouldBe(re5.test(s), expected); }
    for (let i = 0; i < testLoopCount; ++i) { check("abc", true); check("zzz", false); }
    re5.compile("z+", "y");
    for (let i = 0; i < testLoopCount; ++i) { check("zzz", true); check("abc", false); }
})();

// An empty-matchable sticky pattern matches empty AT lastIndex whatever byte is there, so it must
// never be filtered - and a match (even an empty one) must not reset lastIndex to 0. This is what
// the builder's `consumes` propagation protects; every other test here uses a pattern with a minimum
// size of at least 1, so nothing else would notice if that propagation were lost.
(function () {
    // [pattern, expected test("b") at lastIndex 0, expected lastIndex afterwards]
    const emptyMatchable = [
        [/a*/y, true, 0], [/x?/y, true, 0], [/(a)?/y, true, 0], [/(?:)/y, true, 0],
        [/a|/y, true, 0], [/(a*)/y, true, 0], [/\b/y, true, 0],
        [/$/y, false, 0],      // "b" has no end-of-input at index 0
        [/(?=a)/y, false, 0],  // the lookahead fails, so this is a real no-match
    ];
    for (let i = 0; i < testLoopCount; ++i) {
        for (const [re, expectedTest, expectedLastIndex] of emptyMatchable) {
            re.lastIndex = 0;
            shouldBe(re.test("b"), expectedTest);
            shouldBe(re.lastIndex, expectedLastIndex);
        }

        // The same at a non-zero lastIndex: the empty match happens there, so lastIndex stays put.
        const star = /q*/y;
        star.lastIndex = 2;
        shouldBe(star.test("abc"), true);
        shouldBe(star.lastIndex, 2);
    }
})();

// The `gy` combination is filterable too, and takes the same path: a sticky match must begin at
// lastIndex, and RegExpBuiltinExec resets lastIndex to 0 on failure for global as well as for sticky,
// which is what the inline no-match path stores.
(function () {
    const re6 = /\d+/gy;
    const re7 = /[^a-z]/gy;
    for (let i = 0; i < testLoopCount; ++i) {
        re6.lastIndex = 0;
        shouldBe(re6.test("42x"), true);
        shouldBe(re6.lastIndex, 2);
        shouldBe(re6.test("42x"), false);   // 'x' at lastIndex 2 is filtered
        shouldBe(re6.lastIndex, 0);

        re6.lastIndex = 1;
        shouldBe(re6.test("a1"), true);
        shouldBe(re6.lastIndex, 2);

        re7.lastIndex = 0;
        shouldBe(re7.test("Abc"), true);
        shouldBe(re7.lastIndex, 1);
        re7.lastIndex = 0;
        shouldBe(re7.test("abc"), false);   // 'a' filtered
        shouldBe(re7.lastIndex, 0);
    }
})();
