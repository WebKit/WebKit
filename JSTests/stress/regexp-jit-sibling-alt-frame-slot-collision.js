//@ runDefault

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Regression tests for YarrJIT frame-slot aliasing between sibling alternatives
// that mix BackTrackInfoParenthesesOnce (PAC-signed return address) and
// BackTrackInfoParentheses (parenContextHead cleared on backtrack).
// Prior to the fix, these patterns SIGBUSed via failed PAC authentication.

(function() {
    shouldBe(/(a*(|)|()*)*b/.exec("aa"), null, "original repro with 'aa'");
    shouldBe(/(a*(|)|()*)*b/.exec("aab"), ["aab", "aa", "", null], "original repro with 'aab'");
})();

(function() {
    shouldBe(/(a+(|)|()*)*b/.exec("aa"), null, "a+ variant with 'aa'");
})();

(function() {
    shouldBe(/(a*(|)|(a)*)*b/.exec("aa"), null, "nested Once inside alt 1 with 'aa'");
})();

(function() {
    // Extra nesting makes the inner (|) land at a higher frame location, avoiding
    // the collision; this test guards against over-aggressive bail-outs.
    shouldBe(/(a*((|))|()*)*b/.exec("aab"), ["aab", "aa", "", "", null], "extra nesting still matches");
})();

(function() {
    // ()+ has a non-zero minimum, which already triggered a separate JIT bail-out.
    // Keep it here as a control that the surrounding paths still work.
    shouldBe(/(a*(|)|()+)*b/.exec("aa"), null, "()+ variant");
})();
