// Verify the sticky RegExp.exec first-character fast-fail filter: a filtered no-match must behave
// exactly like the operation - reset lastIndex to 0, return null, and leave RegExp's global match
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

// Exercise a no-match at a position whose byte is filtered (not a digit), asserting the whole
// observable contract, including that global RegExp state from a PRIOR success is preserved.
function step() {
    // Prior success at index 1 ("12"), which records global state.
    re.lastIndex = 1;
    let m = re.exec(input);
    shouldBe(m[0], "12");
    shouldBe(m[1], "1");
    shouldBe(m[2], "2");
    shouldBe(m.index, 1);
    shouldBe(re.lastIndex, 3);
    // Capture the global match state established by that success.
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
    shouldBe(re.exec(input), null);
    shouldBe(re.lastIndex, 0);                  // sticky no-match resets lastIndex to 0.
    // Global state must be UNCHANGED by the failed exec.
    shouldBe(RegExp.lastMatch, savedLastMatch);
    shouldBe(RegExp.$1, savedParen1);
    shouldBe(RegExp.$2, savedParen2);
    shouldBe(RegExp.leftContext, savedLeft);
    shouldBe(RegExp.rightContext, savedRight);
    shouldBe(RegExp.input, savedInput);

    // No-match at a filtered position past the last digit ('b' at index 3).
    re.lastIndex = 3;
    shouldBe(re.exec(input), null);
    shouldBe(re.lastIndex, 0);

    // lastIndex beyond the end: null + reset, matching operationRegExpExecStickyKnownRegExp.
    re.lastIndex = 100;
    shouldBe(re.exec(input), null);
    shouldBe(re.lastIndex, 0);

    // A match with the optional second group absent (single trailing digit "3" at index 4).
    re.lastIndex = 4;
    m = re.exec(input);
    shouldBe(m[0], "3");
    shouldBe(m[1], "3");
    shouldBe(m[2], undefined);
    shouldBe(re.lastIndex, 5);
}

for (let i = 0; i < testLoopCount; ++i)
    step();

// Negative lastIndex is not isUInt32; the filter must delegate to the operation, which clamps and
// searches from 0. Here index 0 is 'a' (no match) so the result is null with lastIndex reset.
(function () {
    const re2 = /(\d)(\d)?/y;
    for (let i = 0; i < testLoopCount; ++i) {
        re2.lastIndex = -5;
        shouldBe(re2.exec("a12"), null);
        shouldBe(re2.lastIndex, 0);
    }
})();

// A non-writable lastIndex must make a failing sticky exec throw a TypeError (Set(...,,true)); the
// filter guards on the writable flag and delegates to the operation, which throws.
(function () {
    for (let i = 0; i < testLoopCount; ++i) {
        const re3 = /\d+/y;
        re3.lastIndex = 0;
        Object.defineProperty(re3, "lastIndex", { writable: false });
        shouldThrow(() => re3.exec("abc"), "TypeError");
    }
})();

// 16-bit (non-Latin-1) subject strings must still behave correctly (filter is 8-bit only, so this
// exercises the delegated path). The digit "5" is Latin-1 but the string is 16-bit.
(function () {
    const re4 = /\d/y;
    const wide = "あい" + "5";   // forces a 16-bit string
    for (let i = 0; i < testLoopCount; ++i) {
        re4.lastIndex = 0;
        shouldBe(re4.exec(wide), null);   // first char is a wide non-digit
        shouldBe(re4.lastIndex, 0);
        re4.lastIndex = 2;
        shouldBe(re4.exec(wide)[0], "5");
        shouldBe(re4.lastIndex, 3);
    }
})();
