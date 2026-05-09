// The YARR body-alternative scan must not start a match attempt in the middle of a surrogate
// pair in Unicode (/u and /v) mode. When the last alternative fails at a position whose first
// character is a non-BMP code point, the scan retries the first alternative at the next position.
// Previously the interpreter advanced by a single code unit (landing on the trail surrogate),
// and the ARM64 JIT advanced matchStart by firstCharacterAdditionalReadSize but did not advance
// the index register to match. If the first alternative is a zero-width assertion like \B that
// can match at a trail-surrogate position, this produced a match with end < start.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${String(expected)} but got: ${String(actual)}`);
}

function shouldBeArray(actual, expected) {
    if (!Array.isArray(actual))
        throw new Error(`Expected an array but got: ${String(actual)}`);
    if (actual.length !== expected.length)
        throw new Error(`Expected array of length ${expected.length} but got: ${JSON.stringify(actual)}`);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

// alt1 = \B (zero-width, min size 0), alt2 = x{1,2}? (min size 1).
// At pos 1 (the lead surrogate), alt2 reads the surrogate pair and fails;
// the scan must retry alt1 at pos 3 (the next code-point boundary), never pos 2.
shouldBeArray("a\u{10ffff}b".split(/\B|x{1,2}?/u), ["a\u{10ffff}b"]);
shouldBeArray("a\u{10000}b".split(/\B|x{1,2}?/u), ["a\u{10000}b"]);
shouldBeArray("a\u{10ffff}b".split(/\B|x{1,2}?/v), ["a\u{10ffff}b"]);

// Every code-point boundary in "a\u{10ffff}b" is a word boundary, so there is no match.
shouldBe(/\B|x{1,2}?/u.exec("a\u{10ffff}b"), null);
shouldBe(/\B|x{1,2}?/u.exec("z\u{10000}z"), null);
shouldBe(/\B|x{1,2}?/u.exec("z\u{10ffff}z\u{10000}z"), null);

// Adjacent non-BMP non-word code points: \B must match at the code-point
// boundary between them (pos 3), never at the surrogate middle (pos 2).
{
    let m = /\B|x{1,2}?/u.exec("z\u{10000}\u{10001}z");
    shouldBe(m !== null, true);
    shouldBe(m.index, 3);
    shouldBe(m[0], "");
}

// A longer input mixing BMP, non-BMP, and lone surrogates.
{
    let s = "b7\u{dc00}cu\u{10ffff}\u{dc00}da\u{45d9d}c7k\u{bad30}h";
    let parts = s.split(/\B|x{1,2}?/u);
    shouldBe(parts.join(""), s);
    for (let p of parts) {
        if (p.length > s.length)
            throw new Error(`Piece length ${p.length} exceeds input length ${s.length}`);
    }
}
