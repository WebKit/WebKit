// Coverage for the match-only StringList optimization (capturing fixed-string
// alternations flattened when captures are not observed) and the
// terminal-parentheses optimization generalized to match-only patterns.
//
// Silent on success; throws on the first discrepancy. Runs the whole battery in
// a loop so every JIT tier (and, via the -interpreter companion, the Yarr
// bytecode interpreter) compiles these patterns. The crux of every case: a
// match-only consumer (test/search) must agree with the spec, AND the very same
// source used with a capture-observing consumer (exec/match/replace) must still
// report the correct captures -- proving the flattening never corrupts the
// capturing compile of the same pattern.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("bad value" + (msg ? " (" + msg + ")" : "") + ": " + actual + " expected " + expected);
}

function shouldBeArray(actual, expected, msg) {
    if (actual === null)
        throw new Error("bad value" + (msg ? " (" + msg + ")" : "") + ": null expected " + JSON.stringify(expected));
    if (actual.length !== expected.length)
        throw new Error("bad length" + (msg ? " (" + msg + ")" : "") + ": " + actual.length + " expected " + expected.length);
    for (let i = 0; i < expected.length; ++i) {
        if (actual[i] !== expected[i])
            throw new Error("bad element #" + i + (msg ? " (" + msg + ")" : "") + ": " + actual[i] + " expected " + expected[i]);
    }
}

// A 16-bit (non-Latin1) suffix used to force the subject string to be 16-bit so
// the Char16 matcher paths are exercised too. Patterns that match a prefix can
// match an ASCII prefix of a 16-bit string.
const wide = "あ";

function test() {
    // --- 1. Register-style: each alternative is a single capturing group around a
    //        literal; anchored ^(...)$; exercised match-only via test()/search(). ---
    {
        const re = new RegExp("^((t0)|(t1)|(cfr)|(sp)|(lr)|(pc)|(fr))$");
        for (const s of ["t0", "t1", "cfr", "sp", "lr", "pc", "fr"])
            shouldBe(re.test(s), true, "reg member " + s);
        for (const s of ["", "t", "t2", "zz", "move", "t0x", "xt0", "CFR", "sp ", " sp"])
            shouldBe(re.test(s), false, "reg non-member " + s);
        // search() is also match-only.
        shouldBe(re[Symbol.search]("cfr"), 0, "reg search hit");
        shouldBe("cfr".search(re), 0, "reg search hit 2");
        shouldBe("zz".search(re), -1, "reg search miss");

        // Same source, capture-observing: captures must be exact.
        const m = re.exec("cfr");
        shouldBeArray(m, ["cfr", "cfr", undefined, undefined, "cfr", undefined, undefined, undefined, undefined], "reg exec cfr");
        shouldBe(m.index, 0, "reg exec index");
        shouldBeArray("t1".match(new RegExp("^((t0)|(t1)|(cfr)|(sp)|(lr)|(pc)|(fr))$")),
            ["t1", "t1", undefined, "t1", undefined, undefined, undefined, undefined, undefined], "reg match t1");
        shouldBe("fr".replace(re, "$1/$8"), "fr/fr", "reg replace captures");
    }

    // --- 2. Large generated alternation (like the offlineasm register set). ---
    {
        const regs = [];
        for (let i = 0; i < 6; ++i) regs.push("t" + i, "ft" + i);
        for (let i = 0; i < 10; ++i) regs.push("csr" + i);
        regs.push("cfr", "sp", "lr", "pc", "fr", "a0", "a1", "a2", "a3");
        const re = new RegExp("^((" + regs.join(")|(") + "))$");
        for (const s of regs)
            shouldBe(re.test(s), true, "big member " + s);
        for (const s of ["", "x", "t6", "csr10", "move", "loadi", "storei", "jmp", "bineq"])
            shouldBe(re.test(s), false, "big non-member " + s);
        // exec on a member still reports group 1 (the outer group) correctly.
        const m = re.exec("csr3");
        shouldBe(m[0], "csr3", "big exec[0]");
        shouldBe(m[1], "csr3", "big exec[1]");
    }

    // --- 3. Keyword-style alternation used via test() in a boolean expression. ---
    {
        const kw = new RegExp("^((true)|(false)|(if)|(then)|(else)|(end)|(and)|(or)|(not))$");
        for (const s of ["true", "false", "if", "then", "else", "end", "and", "or", "not"])
            shouldBe(kw.test(s), true, "kw member " + s);
        for (const s of ["tru", "trueX", "Xtrue", "iff", "", "macro"])
            shouldBe(kw.test(s), false, "kw non-member " + s);
    }

    // --- 4. Mixed alternatives: capturing groups, bare strings, non-capturing. ---
    {
        const mix = /^((foo)|bar|(baz)|(?:qux))$/;
        for (const s of ["foo", "bar", "baz", "qux"])
            shouldBe(mix.test(s), true, "mix member " + s);
        for (const s of ["fo", "barX", "quxx", "", "FOO"])
            shouldBe(mix.test(s), false, "mix non-member " + s);
        shouldBeArray(mix.exec("baz"), ["baz", "baz", undefined, "baz"], "mix exec baz");
        shouldBeArray(mix.exec("bar"), ["bar", "bar", undefined, undefined], "mix exec bar");
        shouldBeArray(mix.exec("qux"), ["qux", "qux", undefined, undefined], "mix exec qux");
    }

    // --- 5. Non-EOL prefix string list (no trailing $). Exercises 16-bit subjects. ---
    {
        const pre = /^((alpha)|(beta)|(gamma)|(delta)|(epsilon)|(zeta)|(eta)|(theta))/;
        shouldBe(pre.test("alphaXYZ"), true, "prefix alpha");
        shouldBe(pre.test("thetabeta"), true, "prefix theta");
        shouldBe(pre.test("zzz"), false, "prefix miss");
        shouldBe(pre.test("alph"), false, "prefix partial");
        // 16-bit subject whose ASCII prefix matches.
        shouldBe(pre.test("gamma" + wide), true, "prefix 16-bit gamma");
        shouldBe(pre.test(wide + "gamma"), false, "prefix 16-bit leading-wide");
        shouldBe(pre.exec("delta!")[5], "delta", "prefix exec capture");
    }

    // --- 6. ignore-case. ---
    {
        const ci = new RegExp("^((AA)|(BB)|(CC)|(DD)|(EE)|(FF)|(GG)|(HH))$", "i");
        shouldBe(ci.test("aa"), true, "ci aa");
        shouldBe(ci.test("Cc"), true, "ci Cc");
        shouldBe(ci.test("hh"), true, "ci hh");
        shouldBe(ci.test("zz"), false, "ci zz");
        shouldBeArray(ci.exec("dd"), ["dd", "dd", undefined, undefined, undefined, "dd", undefined, undefined, undefined, undefined], "ci exec dd");
    }

    // --- 7. multiline: ^/$ match at line boundaries; result must stay correct. ---
    {
        const ml = new RegExp("^((aa)|(bb)|(cc)|(dd)|(ee)|(ff)|(gg)|(hh))$", "m");
        shouldBe(ml.test("xx\ncc\nyy"), true, "ml cc");
        shouldBe(ml.test("xxbbyy"), false, "ml no-boundary");
        shouldBe(ml.test("zz\nqq"), false, "ml miss");
        shouldBe(ml.exec("xx\ndd\nyy").index, 3, "ml exec index");
    }

    // --- 8. Empty alternative in the list (matches empty -> always succeeds). ---
    {
        const emptyEOL = /^((aa)|(bb)|()|(cc)|(dd)|(ee)|(ff)|(gg))$/;
        shouldBe(emptyEOL.test(""), true, "emptyEOL empty");
        shouldBe(emptyEOL.test("aa"), true, "emptyEOL aa");
        shouldBe(emptyEOL.test("zz"), false, "emptyEOL zz");
        const emptyNonEOL = /^((aa)|(bb)|()|(cc)|(dd)|(ee)|(ff)|(gg))/;
        shouldBe(emptyNonEOL.test("anything"), true, "emptyNonEOL always");
        shouldBe(emptyNonEOL.test(""), true, "emptyNonEOL empty");
    }

    // --- 9. Fallback shapes that must NOT be flattened but stay correct. ---
    {
        // Backreference -> capture is observable even match-only; no flatten.
        const br = /^((aa)\2|(bb)|(cc)|(dd)|(ee)|(ff)|(gg)|(hh))$/;
        shouldBe(br.test("aaaa"), true, "br aaaa");
        shouldBe(br.test("bb"), true, "br bb");
        shouldBe(br.test("aa"), false, "br aa-only");
        shouldBeArray(br.exec("aaaa"), ["aaaa", "aaaa", "aa", undefined, undefined, undefined, undefined, undefined, undefined, undefined], "br exec");

        // Named groups -> no flatten; captures correct.
        const named = /^((?<x>aa)|(bb)|(cc)|(dd)|(ee)|(ff)|(gg)|(hh))$/;
        shouldBe(named.test("aa"), true, "named aa");
        shouldBe(named.test("zz"), false, "named zz");
        shouldBe(named.exec("aa").groups.x, "aa", "named group x");

        // Quantified inner group -> not a pure fixed string; falls back.
        const quant = /^((t0)|(t1)?|(cc)|(dd)|(ee)|(ff)|(gg)|(hh))$/;
        shouldBe(quant.test("t0"), true, "quant t0");
        shouldBe(quant.test("t1"), true, "quant t1");
        shouldBe(quant.test(""), true, "quant empty (t1? empty)");
        shouldBe(quant.test("zz"), false, "quant zz");

        // Deeper nesting inside an alternative -> not unwrappable; falls back.
        const deep = /^(((ab))|(cd)|(ee)|(ff)|(gg)|(hh)|(ii))$/;
        shouldBe(deep.test("ab"), true, "deep ab");
        shouldBe(deep.test("cd"), true, "deep cd");
        shouldBe(deep.test("zz"), false, "deep zz");
        shouldBe(deep.exec("ab")[3], "ab", "deep inner capture");
    }

    // --- 10. Terminal-parentheses optimization in match-only with captures elsewhere. ---
    {
        // Trailing greedy non-capturing group; capture (\d+) elsewhere.
        const t1 = /(\d+)x(?:ab)*/;
        shouldBe(t1.test("12xababab"), true, "term t1 a");
        shouldBe(t1.test("7x"), true, "term t1 b");
        shouldBe(t1.test("nope"), false, "term t1 c");
        shouldBeArray(t1.exec("12xabab"), ["12xabab", "12"], "term t1 exec");

        // Multi-alternative terminal group.
        const t2 = /(\w)(?:ab|cd)*/;
        shouldBe(t2.test("zabcdab"), true, "term t2 a");
        shouldBe(t2.test("z"), true, "term t2 b");
        shouldBe(t2.exec("zabcd")[1], "z", "term t2 exec");

        // Terminal candidate that CONTAINS a nested capture: must stay correct
        // (guard keeps it off the terminal fast path).
        const t3 = /q(?:(a)b)*/;
        shouldBe(t3.test("qababab"), true, "term t3 a");
        shouldBe(t3.test("q"), true, "term t3 b");
        shouldBeArray(t3.exec("qabab"), ["qabab", "a"], "term t3 exec");

        // Capturing trailing group must not be terminal-marked; captures correct.
        const t4 = /m(\d)*/;
        shouldBe(t4.test("m123"), true, "term t4 a");
        shouldBe(t4.exec("m123")[1], "3", "term t4 exec last iter");

        // No-capture trailing greedy (the originally-supported shape).
        const t5 = /foo(?:x)*/;
        shouldBe(t5.test("fooxxxx"), true, "term t5 a");
        shouldBe(t5.test("bar"), false, "term t5 b");
    }

    // --- 11. Single-character alternatives (length-1 fixed strings). ---
    {
        const ch = /^((a)|(b)|(c)|(d)|(e)|(f)|(g)|(h))$/;
        for (const s of ["a", "c", "h"])
            shouldBe(ch.test(s), true, "ch member " + s);
        for (const s of ["", "z", "ab"])
            shouldBe(ch.test(s), false, "ch non-member " + s);
        shouldBeArray(ch.exec("c"), ["c", "c", undefined, undefined, "c", undefined, undefined, undefined, undefined, undefined], "ch exec c");
    }

    // --- 12. Varying-length EOL alternatives (length-sort path). ---
    {
        const vl = /^((a)|(bb)|(ccc)|(dddd)|(ee)|(f)|(gggg)|(hh))$/;
        for (const s of ["a", "bb", "ccc", "dddd", "ee", "f", "gggg", "hh"])
            shouldBe(vl.test(s), true, "vl member " + s);
        for (const s of ["", "aa", "cccc", "ddddd", "z"])
            shouldBe(vl.test(s), false, "vl non-member " + s);
    }
}

for (let i = 0; i < 200; ++i)
    test();
