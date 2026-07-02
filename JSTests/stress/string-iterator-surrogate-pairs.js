function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

// Independent reference: step through the string using codePointAt and slice with substring.
function referenceSegments(s) {
    var r = [];
    var i = 0;
    while (i < s.length) {
        var cp = s.codePointAt(i);
        var len = cp > 0xFFFF ? 2 : 1;
        r.push(s.substring(i, i + len));
        i += len;
    }
    return r;
}
noInline(referenceSegments);

function forOfSegments(s) {
    var r = [];
    for (var c of s)
        r.push(c);
    return r;
}
noInline(forOfSegments);

function manualSegments(s) {
    var it = s[Symbol.iterator]();
    var r = [];
    for (;;) {
        var n = it.next();
        if (n.done)
            break;
        r.push(n.value);
    }
    return r;
}
noInline(manualSegments);

function dump(s) {
    var u = [];
    for (var i = 0; i < s.length; ++i)
        u.push(s.charCodeAt(i).toString(16));
    return u.join(" ");
}

function check(s) {
    var ref = referenceSegments(s);
    var got = forOfSegments(s);
    assert(got.length === ref.length, "for-of segment count mismatch for [" + dump(s) + "]: " + got.length + " vs " + ref.length);
    for (var i = 0; i < ref.length; i++)
        assert(got[i] === ref[i], "for-of segment " + i + " mismatch for [" + dump(s) + "]: [" + dump(got[i]) + "] vs [" + dump(ref[i]) + "]");
    assert(got.join("") === s, "for-of join mismatch for [" + dump(s) + "]");

    var man = manualSegments(s);
    assert(man.length === ref.length, "manual next() segment count mismatch for [" + dump(s) + "]");
    for (var i = 0; i < ref.length; i++)
        assert(man[i] === ref[i], "manual next() segment " + i + " mismatch for [" + dump(s) + "]");
    assert(man.join("") === s, "manual next() join mismatch for [" + dump(s) + "]");
}

// Hard-coded expectations for the canonical surrogate cases.
function checkExact(s, expected) {
    var got = forOfSegments(s);
    assert(got.length === expected.length, "exact count mismatch for [" + dump(s) + "]");
    for (var i = 0; i < expected.length; i++)
        assert(got[i] === expected[i], "exact segment " + i + " mismatch for [" + dump(s) + "]");
    check(s);
}

function runExactCases() {
    checkExact("", []);
    checkExact("a", ["a"]);
    checkExact("𠮷", ["𠮷"]);
    checkExact("a𠮷", ["a", "𠮷"]);
    checkExact("𠮷a", ["𠮷", "a"]);
    checkExact("\uD842", ["\uD842"]);
    checkExact("a\uD842", ["a", "\uD842"]);
    checkExact("\uD842x", ["\uD842", "x"]);
    checkExact("\uD842\uD842", ["\uD842", "\uD842"]);
    checkExact("\uD842𠮷", ["\uD842", "𠮷"]);
    checkExact("\uDFB7", ["\uDFB7"]);
    checkExact("\uDFB7x", ["\uDFB7", "x"]);
    checkExact("\uDFB7\uD842", ["\uDFB7", "\uD842"]);
    checkExact("\uDC00\uDC00", ["\uDC00", "\uDC00"]);
    checkExact("\u{10000}", ["\u{10000}"]);
    checkExact("\u{10FFFF}", ["\u{10FFFF}"]);
    checkExact("\uD7FF\uDC00", ["\uD7FF", "\uDC00"]);
    checkExact("\uDC00\uD800", ["\uDC00", "\uD800"]);
    checkExact("\uE000\uDFFF", ["\uE000", "\uDFFF"]);
}
noInline(runExactCases);

// All 1 / 2 / 3 length combinations of the interesting boundary code units.
var units = [0x0041, 0x00FF, 0x0100, 0xD7FF, 0xD800, 0xDBFF, 0xDC00, 0xDFFF, 0xE000, 0xFFFF];
function runBoundaryCombinations() {
    for (var a = 0; a < units.length; ++a) {
        check(String.fromCharCode(units[a]));
        for (var b = 0; b < units.length; ++b) {
            check(String.fromCharCode(units[a], units[b]));
            for (var c = 0; c < units.length; ++c)
                check(String.fromCharCode(units[a], units[b], units[c]));
        }
    }
}
noInline(runBoundaryCombinations);

// Deterministic pseudo-random strings mixing ASCII / Latin-1 / BMP / lone and paired surrogates.
var seed = 42;
function rnd(n) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed % n;
}
function randomUnit() {
    var k = rnd(10);
    if (k < 4) return 0x20 + rnd(0x60);
    if (k < 6) return rnd(0x100);
    if (k < 8) return rnd(0x10000);
    if (k < 9) return 0xD800 + rnd(0x400);
    return 0xDC00 + rnd(0x400);
}
function runRandomStrings() {
    for (var iteration = 0; iteration < 500; ++iteration) {
        var len = rnd(30);
        var arr = [];
        for (var i = 0; i < len; ++i)
            arr.push(randomUnit());
        check(String.fromCharCode.apply(null, arr));
    }
    for (var iteration = 0; iteration < 50; ++iteration) {
        var rope = "abc" + "\u{20BB7}".repeat(1 + rnd(5)) + String.fromCharCode(0xD800 + rnd(0x400)) + "xyz".repeat(rnd(3));
        check(rope);
        check(rope.substring(1, rope.length - 1));
    }
}
noInline(runRandomStrings);

runExactCases();
runBoundaryCombinations();
runRandomStrings();

// Keep iterating a fixed surrogate-heavy set so the for-of site tiers up with surrogate pairs
// flowing through the optimized code, verifying every iteration.
var hot = [
    "\u{1F600}\u{1F601}\u{1F602}\u{1F603}",
    "a\uD800b\uDC00c\u{10000}",
    "\u{20BB7}野家\u{29E3D}",
    "\uD800\uD800\uD800\uD800\uD800",
    "\uDC00\uDC00\uDC00\uDC00\uDC00",
    ("ab\u{20BB7}").repeat(20),
];
for (var i = 0; i < testLoopCount; ++i)
    check(hot[i % hot.length]);
runExactCases();
