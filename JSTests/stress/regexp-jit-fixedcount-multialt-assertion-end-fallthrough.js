//@ runDefault

// Regression test for a YarrJIT crash on patterns where a FixedCount multi-alt
// parenthesis has an alternative whose content ends with a ParentheticalAssertion.
//
// The FixedCount multi-alt mechanism stores a "content backtrack" return address
// per alternative. When the alternative that just matched completes, its return
// address is patched to the next sibling alternative's content-backtrack entry
// label, which is positioned immediately after the sibling's backtrack code.
// That position then happens to be the first instruction emitted for the
// assertion's inner disjunction backtrack (a dispatch via frame slot that holds
// the inner alt's resume address).
//
// When the assertion "succeeded" on the forward path via the assertion-success
// branch (an inverted lookahead whose inner disjunction failed), that frame
// slot was never written, so dispatching via it jumped to garbage. Fix is to
// emit an unconditional jump at ParentheticalAssertionEnd.bt so that any
// control flow reaching that position is redirected past the assertion via
// ParentheticalAssertionBegin.bt's End-jumps list.

function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: actual=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

for (var i = 0; i < 1e4; ++i) {
    // Minimal reproducer: inverted lookahead with inner disjunction inside a
    // FixedCount multi-alt paren. Pre-fix, the JIT would SIGSEGV/SIGBUS here.
    shouldBe(/(^(?!(X|Y))c|Zc){2}/.exec("cccccc"), null);
    shouldBe(/(^(?!(X|Y))c|Zc){3}/.exec("cccccc"), null);
    shouldBe(/(^(?!(X|Y))c|(Z)c){3}/.exec("cccccc"), null);
    shouldBe(/(^(?!d(X|Y))c|(Z)c){3}/.exec("cccccc"), null);
    shouldBe(/(^(?!(XX|YY))c|Zc){2}/.exec("cccccc"), null);
    shouldBe(/(^(?!(?:X|Y))c|Zc){2}/.exec("cccccc"), null);

    // Valid matches must still succeed.
    shouldBe(/(^(?!(X|Y))c|Zc){2}/.exec("ZcZc"), ["ZcZc", "Zc", null]);
    shouldBe(/(^(?!(X|Y))c|Zc){2}/.exec("cZc"), ["cZc", "Zc", null]);

    // Original crashing test input.
    var r = new RegExp(unescape(
        "%28%5E%28%3F%21%64%28%28%3D%28%28%7E%58%28%3F%3C%E2%3E%28%3F%3C%66%3E" +
        "%29%28%28%28%28%2C%28%28%28%28%28%29%29%29%29%29%29%29%29%29%28%28%28" +
        "%28%28%28%28%28%28%3E%28%28%28%28%29%29%29%28%28%29%28%29%29%29%29%29" +
        "%29%29%29%29%29%29%29%29%29%29%29%7C%28%28%28%28%29%28%28%28%28%28%28" +
        "%28%28%3F%3C%E2%3E%28%28%4D%29%29%29%29%29%29%29%29%29%28%28%28%28%28" +
        "%28%29%29%29%29%29%29%28%28%28%28%3F%3C%66%3E%28%28%29%29%29%29%29%29" +
        "%29%28%29%29%28%29%28%28%28%28%28%28%28%28%28%28%28%28%3F%3C%66%35%32" +
        "%73%3E%28%28%28%29%29%29%28%29%28%28%28%28%29%29%29%29%29%28%28%29%28" +
        "%29%29%29%29%29%29%29%29%A9%29%29%29%29%29%29%29%29%29%63%7C%28%3E%29" +
        "%63%7C%28%3A%29%28%28%29%29%29%7B%33%7D%FF%FF%FF%FF%3C"), "dis");
    "aabbbccc".match(r);
    "aabbbccc".replace(r, "$1,$2");
    r.exec("caffeeeee");
    "cccccccc".match(r);
}
