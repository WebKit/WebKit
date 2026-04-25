function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

// Shallow rope (resolved + resolved), no position arg.
{
    let base = "A".padEnd(500, "x");
    base.charCodeAt(0);
    function sw(a, b, c) { return (a + b).startsWith(c); }
    noInline(sw);
    function ew(a, b, c) { return (a + b).endsWith(c); }
    noInline(ew);
    for (let i = 0; i < 1e4; ++i) {
        shouldBe(sw(base, "TAIL", "A"), true);
        shouldBe(sw(base, "TAIL", "x"), false);
        shouldBe(sw(base, "TAIL", "T"), false);
        shouldBe(ew(base, "TAIL", "L"), true);
        shouldBe(ew(base, "TAIL", "x"), false);
        shouldBe(ew(base, "TAIL", "I"), false);
    }
}

// With position / endPosition (DFG WithIndex / WithEndPosition operations).
{
    let base = "ABC".padEnd(500, "x");
    base.charCodeAt(0);
    function swp(a, b, c, p) { return (a + b).startsWith(c, p); }
    noInline(swp);
    function ewp(a, b, c, p) { return (a + b).endsWith(c, p); }
    noInline(ewp);
    for (let i = 0; i < 1e4; ++i) {
        shouldBe(swp(base, "TAIL", "B", 1), true);
        shouldBe(swp(base, "TAIL", "B", 2), false);
        shouldBe(swp(base, "TAIL", "L", 503), true);
        shouldBe(swp(base, "TAIL", "?", 999), false);
        shouldBe(swp(base, "TAIL", "A", -5), true);
        shouldBe(ewp(base, "TAIL", "A", 1), true);
        shouldBe(ewp(base, "TAIL", "B", 1), false);
        shouldBe(ewp(base, "TAIL", "?", 0), false);
        shouldBe(ewp(base, "TAIL", "?", -5), false);
        shouldBe(ewp(base, "TAIL", "L", 999), true);
    }
}

// Nested rope: first fiber is itself a rope -> tryGetCharAt returns nullopt
// for index 0 (slow-path fallback), but last fiber is resolved so endsWith
// still takes the fast path.
{
    let base = "A".padEnd(500, "x");
    base.charCodeAt(0);
    function sw(r, c) { return r.startsWith(c); }
    noInline(sw);
    function ew(r, c) { return r.endsWith(c); }
    noInline(ew);
    for (let i = 0; i < 1e4; ++i) {
        let deep = (base + "Y") + "Z";
        shouldBe(sw(deep, "A"), true);
        shouldBe(sw(deep, "Z"), false);
        shouldBe(ew(deep, "Z"), true);
        shouldBe(ew(deep, "Y"), false);
    }
}

// Substring rope as receiver (top-level isSubstring branch).
{
    let big = "Q".padEnd(1000, "abc");
    big.charCodeAt(0);
    function sw(s, c) { return s.startsWith(c); }
    noInline(sw);
    function ew(s, c) { return s.endsWith(c); }
    noInline(ew);
    for (let i = 0; i < 1e4; ++i) {
        let sub = big.substring(1, 700);
        shouldBe(sw(sub, "a"), true);
        shouldBe(sw(sub, "Q"), false);
        shouldBe(ew(sub, "c"), true);
        shouldBe(ew(sub, "a"), false);
    }
}

// 16-bit search character.
{
    let base = "\u3042".padEnd(500, "x");
    base.charCodeAt(0);
    function sw(a, b, c) { return (a + b).startsWith(c); }
    noInline(sw);
    function ew(a, b, c) { return (a + b).endsWith(c); }
    noInline(ew);
    for (let i = 0; i < 1e4; ++i) {
        shouldBe(sw(base, "\u3044", "\u3042"), true);
        shouldBe(sw(base, "\u3044", "\u3044"), false);
        shouldBe(ew(base, "\u3044", "\u3044"), true);
        shouldBe(ew(base, "\u3044", "x"), false);
    }
}
