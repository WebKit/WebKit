// Test forward references in regular expressions.
// Forward references refer to capture groups that appear later in the pattern.
// Per ES spec (22.2.2.7.2 BackreferenceMatcher step 1.7), when the referenced
// capture is undefined (not yet captured), the backreference matches the empty
// string. This tests the Yarr JIT implementation of forward references.

function testExec(re, str, expected, description) {
    let match = re.exec(str);

    if (expected === null) {
        if (match !== null)
            throw new Error(`${description}: expected no match, but got ${JSON.stringify(match)}`);
        return;
    }

    if (match === null)
        throw new Error(`${description}: expected ${JSON.stringify(expected)}, but got no match`);

    if (match[0] !== expected[0])
        throw new Error(`${description}: expected match[0]="${expected[0]}", got "${match[0]}"`);

    for (let i = 1; i < expected.length; i++) {
        if (match[i] !== expected[i])
            throw new Error(`${description}: expected match[${i}]="${expected[i]}", got "${match[i]}"`);
    }
}

function testTest(re, str, expected, description) {
    let result = re.test(str);
    if (result !== expected)
        throw new Error(`${description}: ${re}.test("${str}") = ${result}, expected ${expected}`);
}

// ==== Basic numbered forward references ====

testTest(/\1(a)/, "a", true, "basic numbered forward ref");
testTest(/\1(a)/, "b", false, "basic numbered forward ref no match");
testExec(/\1(a)/, "a", ["a", "a"], "basic numbered forward ref exec");

testTest(/\1(.)/, "x", true, "forward ref dot");
testExec(/\1(.)/, "x", ["x", "x"], "forward ref dot exec");

// Forward ref matches empty, so only the group content needs to match
testExec(/\1(ab)/, "ab", ["ab", "ab"], "forward ref multi-char group");
testTest(/\1(ab)/, "xab", true, "forward ref multi-char group with prefix");

// ==== Basic named forward references ====

testTest(/\k<a>(?<a>x)/, "x", true, "basic named forward ref");
testTest(/\k<a>(?<a>x)/, "y", false, "basic named forward ref no match");
testExec(/\k<a>(?<a>x)/, "x", ["x", "x"], "basic named forward ref exec");

testExec(/\k<grp>(?<grp>hello)/, "hello", ["hello", "hello"], "named forward ref word");
testTest(/\k<grp>(?<grp>hello)/, "world", false, "named forward ref word no match");

// ==== Forward reference then back reference ====
// \k<a> is a forward ref (empty match), (?<a>x) captures "x", \k<a> is now a back ref matching "x"

testExec(/\k<a>(?<a>x)\k<a>/, "xx", ["xx", "x"], "forward ref then back ref");
testTest(/\k<a>(?<a>x)\k<a>/, "xy", false, "forward ref then back ref no match");
testTest(/\k<a>(?<a>x)\k<a>/, "x", false, "forward ref then back ref too short");

testExec(/\1(abc)\1/, "abcabc", ["abcabc", "abc"], "numbered forward then back ref");
testTest(/\1(abc)\1/, "abcdef", false, "numbered forward then back ref no match");

// ==== Multiple forward references ====

testExec(/\1\2(a)(b)/, "ab", ["ab", "a", "b"], "two forward refs");
testExec(/\1\2(a)(b)\1\2/, "abab", ["abab", "a", "b"], "two forward refs then back refs");
testTest(/\1\2(a)(b)\1\2/, "abba", false, "two forward refs then back refs no match");

testExec(/\k<x>\k<y>(?<x>A)(?<y>B)\k<x>\k<y>/, "ABAB",
    ["ABAB", "A", "B"], "two named forward refs then back refs");

// ==== Forward reference with quantifiers on the group ====

testExec(/\1([a-z]+)/, "hello", ["hello", "hello"], "forward ref with + group");
testExec(/\1([a-z]*)/, "hello", ["hello", "hello"], "forward ref with * group");
testExec(/\1([a-z]?)/, "h", ["h", "h"], "forward ref with ? group");
testExec(/\1([a-z]?)/, "", ["", ""], "forward ref with ? group empty");

// ==== Forward reference with quantifiers on the reference ====

testExec(/\1*(a)/, "a", ["a", "a"], "forward ref with * quantifier");
testExec(/\1+(a)/, "a", ["a", "a"], "forward ref with + quantifier");
testExec(/\1?(a)/, "a", ["a", "a"], "forward ref with ? quantifier");
testExec(/\1{0,3}(a)/, "a", ["a", "a"], "forward ref with {0,3} quantifier");

// ==== Forward reference in alternation ====

testExec(/\1(a)|b/, "a", ["a", "a"], "forward ref in alternation left");
testExec(/\1(a)|b/, "b", ["b", undefined], "forward ref in alternation right");
testExec(/c|\1(a)/, "a", ["a", "a"], "forward ref in alternation right side");

// ==== Forward reference with anchors ====

testExec(/^\1(abc)$/, "abc", ["abc", "abc"], "forward ref with anchors");
testTest(/^\1(abc)$/, "xabc", false, "forward ref with anchors no match");
testTest(/^\1(abc)$/, "abcx", false, "forward ref with anchors no match 2");

// ==== Forward reference with character classes ====

testExec(/\1([aeiou])/, "e", ["e", "e"], "forward ref with char class");
testExec(/\1([0-9]{3})/, "123", ["123", "123"], "forward ref with digit range");
testExec(/\1([^abc])/, "x", ["x", "x"], "forward ref with negated class");

// ==== Forward reference with case insensitive flag ====

testTest(/\k<a>(?<a>x)\k<a>/i, "xX", true, "forward ref case insensitive");
testTest(/\k<a>(?<a>x)\k<a>/i, "xx", true, "forward ref case insensitive same case");
testExec(/\1(A)\1/i, "AaA".toLowerCase(), ["aa", "a"], "numbered forward ref case insensitive");
testTest(/\1(A)\1/i, "Aa", true, "numbered forward ref case insensitive mixed");

// ==== Forward reference with unicode flag ====

testTest(/\1(.)/u, "a", true, "forward ref unicode mode");
testExec(/\k<a>(?<a>.)/u, "\u{1F600}", ["\u{1F600}", "\u{1F600}"], "forward ref unicode emoji");
testExec(/\1(.)\1/u, "\u{1F600}\u{1F600}", ["\u{1F600}\u{1F600}", "\u{1F600}"],
    "forward then back ref unicode emoji");

// ==== Forward reference with dotAll flag ====

testExec(/\1(.)/s, "\n", ["\n", "\n"], "forward ref dotAll newline");
testExec(/\1(.)\1/s, "\n\n", ["\n\n", "\n"], "forward then back ref dotAll");

// ==== Forward reference with multiline flag ====

testTest(/^\1(a)/m, "a", true, "forward ref multiline");
testTest(/^\1(a)$/m, "\na", true, "forward ref multiline second line");

// ==== Forward reference in lookahead ====

testTest(/(?=\1(a))a/, "a", true, "forward ref in positive lookahead");
testTest(/(?!\1(b))a/, "a", true, "forward ref in negative lookahead");

// ==== Nested groups with forward reference ====

testExec(/\1((ab))/, "ab", ["ab", "ab", "ab"], "forward ref to nested group");
testExec(/\2(a(b))/, "ab", ["ab", "ab", "b"], "forward ref to inner nested group");

// ==== Forward reference with non-capturing groups ====

testExec(/\1(?:x)(a)/, "xa", ["xa", "a"], "forward ref with non-capturing group");
testExec(/(?:\1(a))/, "a", ["a", "a"], "forward ref inside non-capturing group");

// ==== Complex patterns ====

// Pattern: forward ref, literal, capture, back ref
testExec(/\k<z>X(?<z>z*)X\k<z>/, "XzzXzz", ["XzzXzz", "zz"],
    "forward ref + literal + capture + back ref");
testTest(/\k<z>X(?<z>z*)X\k<z>/, "XzzXz", false,
    "forward ref + literal + capture + back ref no match");

// Multiple forward references to different groups, then back references
testExec(/\k<a>\k<b>(?<a>[A-Z]+)(?<b>[0-9]+)\k<a>\k<b>/, "ABC123ABC123",
    ["ABC123ABC123", "ABC", "123"],
    "multiple named forward refs then back refs complex");

// Forward reference in a repeated group
// Note: captures inside quantified groups are reset to undefined at each iteration
// (ES spec 22.2.2.5.1 RepeatMatcher step 1.6), so \1 matches empty on both iterations.
testExec(/(?:\1(a)){2}/, "aa", ["aa", "a"], "forward ref in repeated group");

// ==== Interaction with optional groups ====

// Forward ref to optional group that does match
testExec(/\1(a)?b/, "ab", ["ab", "a"], "forward ref to optional group that matches");
// Forward ref to optional group that doesn't match
testExec(/\1(a)?b/, "b", ["b", undefined], "forward ref to optional group unmatched");

// ==== Stress: ensure JIT handles forward refs correctly under repeated execution ====

for (let i = 0; i < testLoopCount; i++) {
    let r1 = /\k<a>(?<a>x)/.test("x");
    if (!r1)
        throw new Error("JIT stress test 1 failed at iteration " + i);

    let r2 = /\1(a)\1/.test("aa");
    if (!r2)
        throw new Error("JIT stress test 2 failed at iteration " + i);

    let r3 = /\k<a>(?<a>.)X\k<a>/.test("aXa");
    if (!r3)
        throw new Error("JIT stress test 3 failed at iteration " + i);

    let r4 = /\k<a>(?<a>.)X\k<a>/.test("aXb");
    if (r4)
        throw new Error("JIT stress test 4 (should not match) failed at iteration " + i);
}

// ==== Stress: forward reference with String.prototype.match/replace/split ====

let m = "hello".match(/\1([a-z]+)/);
if (m[0] !== "hello" || m[1] !== "hello")
    throw new Error("String.prototype.match with forward ref failed");

let r = "abc".replace(/\1(b)/, "X");
if (r !== "aXc")
    throw new Error("String.prototype.replace with forward ref failed: " + r);

let s = "aXbXc".split(/\1(X)/);
if (s[0] !== "a" || s[1] !== "X" || s[2] !== "b" || s[3] !== "X" || s[4] !== "c")
    throw new Error("String.prototype.split with forward ref failed: " + JSON.stringify(s));

// ==== Stress: forward reference with global/sticky flags ====

let gre = /\1(a)/g;
let gmatches = "aaa".match(gre);
if (gmatches.length !== 3 || gmatches[0] !== "a" || gmatches[1] !== "a" || gmatches[2] !== "a")
    throw new Error("global flag with forward ref failed: " + JSON.stringify(gmatches));

let yre = /\1(a)/y;
if (!yre.test("a"))
    throw new Error("sticky flag with forward ref failed");
