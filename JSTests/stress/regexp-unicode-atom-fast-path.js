function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

// Multi-char atom under unicode flag.
{
    let str = "ab cd ab cd ab";
    shouldBeArray(str.match(/ab/gu), ["ab", "ab", "ab"]);
    shouldBeArray(str.match(/ab/gv), ["ab", "ab", "ab"]);
    shouldBe(str.replace(/ab/gu, "X"), "X cd X cd X");
    shouldBeArray(str.split(/ab/u), ["", " cd ", " cd ", ""]);
}

// Single-char atom under unicode flag.
{
    let str = "aaa";
    shouldBeArray(str.match(/a/gu), ["a", "a", "a"]);
    shouldBe(str.search(/a/u), 0);
    shouldBe("xyz".search(/a/u), -1);
}

// exec / test fast path.
{
    let re = /ab/u;
    let m = re.exec("xxabyy");
    shouldBe(m.index, 2);
    shouldBe(m[0], "ab");
    shouldBe(/ab/u.test("xxabyy"), true);
    shouldBe(/ab/u.test("xxaayy"), false);
}

// 16-bit input with surrogate pairs: atom must not match inside a surrogate pair.
{
    // U+10000 is "𐀀". Atom "ab" must not be confused.
    let str = "\u{10000}ab\u{10000}ab";
    shouldBeArray(str.match(/ab/gu), ["ab", "ab"]);
    shouldBe(str.replace(/ab/gu, ""), "\u{10000}\u{10000}");
}

// Non-BMP fixed string: must NOT take atom fast path (still correct via Yarr).
{
    let str = "\u{1F600}x\u{1F600}";
    shouldBeArray(str.match(/\u{1F600}/gu), ["\u{1F600}", "\u{1F600}"]);
}

// Lone-surrogate pattern: must NOT take atom fast path under unicode.
// In unicode mode, /\uD800/u does NOT match the lead surrogate of a paired surrogate.
{
    let paired = "𐀀"; // U+10000
    shouldBe(/\uD800/u.test(paired), false);
    shouldBe(/\uD800/u.test("\uD800x"), true);
    shouldBe(paired.match(/\uD800/gu), null);
}

// Lone trail surrogate.
{
    let paired = "𐀀";
    shouldBe(/\uDC00/u.test(paired), false);
    shouldBe(/\uDC00/u.test("x\uDC00"), true);
}

// Non-unicode lone surrogate: still takes atom path (no surrogate-pair semantics).
{
    let paired = "𐀀";
    shouldBe(/\uD800/.test(paired), true);
    shouldBeArray(paired.match(/\uD800/g), ["\uD800"]);
}

// RegExp.$_ etc. should reflect the last match.
{
    "xxabyy".match(/ab/gu);
    shouldBe(RegExp.lastMatch, "ab");
    shouldBe(RegExp.leftContext, "xx");
    shouldBe(RegExp.rightContext, "yy");
}

// 16-bit BMP atom (non-ASCII, non-surrogate).
{
    let str = "あいあい";
    shouldBeArray(str.match(/あ/gu), ["あ", "あ"]);
    shouldBeArray(str.match(/あい/gu), ["あい", "あい"]);
}

// Boundary BMP characters adjacent to surrogate range.
{
    shouldBeArray("퟿퟿".match(/퟿/gu), ["퟿", "퟿"]);
    shouldBeArray("".match(//gu), ["", ""]);
}

// Input contains broken trail surrogate before atom.
{
    shouldBeArray("\uDC00ab".match(/ab/gu), ["ab"]);
    shouldBe("\uDC00ab".search(/ab/u), 1);
}

// Mixed BMP-and-surrogate atom must NOT be extracted.
{
    shouldBeArray("a\uD800x".match(/a\uD800/gu), ["a\uD800"]);
    shouldBe("a\u{10000}x".match(/a\uD800/gu), null);
}

// dotAll flag does not affect fixed-string atoms.
{
    shouldBeArray("a.b a.b".match(/a/gsu), ["a", "a"]);
}

// matchAll iterator.
{
    let indices = [];
    for (let m of "ab-ab".matchAll(/ab/gu))
        indices.push(m.index);
    shouldBeArray(indices, [0, 3]);
}

// replace and replaceAll.
{
    shouldBe("ab-ab-ab".replace(/ab/gu, "X"), "X-X-X");
    shouldBe("ab-ab-ab".replaceAll(/ab/gu, ""), "--");
    shouldBe("ab-ab".replace(/ab/u, "X"), "X-ab");
}

// JIT tier-up consistency.
{
    function f(s) { return s.match(/ab/gu); }
    noInline(f);
    for (let i = 0; i < 10000; i++) {
        let r = f("xxab--ab");
        if (r.length !== 2 || r[0] !== "ab" || r[1] !== "ab")
            throw new Error("JIT tier-up mismatch at " + i);
    }
}
