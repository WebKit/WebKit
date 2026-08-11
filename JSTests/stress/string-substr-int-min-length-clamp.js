// Regression test for https://bugs.webkit.org/show_bug.cgi?id=317480
//
// The DFG's inlined String.prototype.substr clamps the length with
// size = max(0, min(length, len - start)). On x86_64 the second clamp lowered
// to `test; xorl reg, reg; cmov`, and the flag-clobbering xorl (materializing
// the zero immediate) corrupted the sign flag the cmov depended on. A negative
// length such as INT_MIN was therefore not clamped to zero and produced an
// invalid JSString with length 2147483648.

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`Bad value: ${actual}, expected: ${expected}`);
}

function f(s, n) {
    return s.substr(2, n);
}
noInline(f);

const s = "x".repeat(64);
for (let i = 0; i < testLoopCount; ++i) {
    const r = f(s, -2147483648);
    shouldBe(r.length, 0);
    shouldBe(r, "");
}

// Materializing a bounded substring from the (previously malformed) result must
// not expose any code units beyond the actual source extent.
const bad = f(s, -2147483648);
const copied = bad.substr(0, 80);
shouldBe(bad.length, 0);
shouldBe(copied.length, 0);
shouldBe(copied, "");
