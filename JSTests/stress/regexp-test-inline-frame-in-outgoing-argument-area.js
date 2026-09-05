//@ requireOptions("--maximumRegExpTestInlineCodesize=10000")

// The Yarr frame of an inlined RegExp#test lives in the outgoing-argument area of the
// DFG/FTL frame. Two inlined patterns with different frame sizes, JS calls that lay out
// their arguments in the same area, and locals live across all of them must not
// interfere with each other.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

function callee(a, b, c, d, e, f, g) {
    return a + b + c + d + e + f + g;
}
noInline(callee);

function test(s, k, args) {
    let a0 = k + 1, a1 = k + 2, a2 = k + 3, a3 = k + 4, a4 = k + 5, a5 = k + 6, a6 = k + 7, a7 = k + 8;
    let b0 = k * 1.5, b1 = k * 2.5, b2 = k * 3.5, b3 = k * 4.5;
    let r = 0;
    r += /^[a-z]+$/u.test(s) ? 1 : 0;                                                          // 2 frame slots
    r += /^[a-c][a-c][a-c][a-c][a-c][a-c][a-c][a-c][a-c][a-c][a-c][a-c]$/u.test(s) ? 2 : 0;    // 24 frame slots
    r += callee(k, k, k, k, k, k, k);
    r += callee.apply(null, args);
    shouldBe(a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + b0 + b1 + b2 + b3, 20 * k + 36, "locals for k=" + k);
    return r;
}
noInline(test);

const args = [1, 1, 1, 1, 1, 1, 1];
for (let i = 0; i < testLoopCount; ++i) {
    const k = i & 0xff;
    const s = i & 1 ? "abcabcabcabc" : "abcabcabcabd!";
    shouldBe(test(s, k, args), 7 * k + 7 + (i & 1 ? 3 : 0), "result for i=" + i);
}
