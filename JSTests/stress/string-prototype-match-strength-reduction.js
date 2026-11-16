// Exercises the DFG path where a StringMatch node is converted to RegExpMatchFast
// in fixup and then further strength-reduced to RegExpMatchFastGlobal /
// RegExpExecNonGlobalOrSticky for known-flag literal RegExp arguments.

function shouldBe(actual, expected) {
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("expected " + e + " but got " + a);
}

// Global literal: StringMatch -> RegExpMatchFast -> RegExpMatchFastGlobal
function matchGlobalLiteral(str) {
    return str.match(/[0-9]/g);
}
noInline(matchGlobalLiteral);
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchGlobalLiteral("a1b2c3"), ["1", "2", "3"]);
shouldBe(matchGlobalLiteral("nope"), null);

// Non-global literal: StringMatch -> RegExpMatchFast -> RegExpExec
function matchNonGlobalLiteral(str) {
    return str.match(/[0-9]/);
}
noInline(matchNonGlobalLiteral);
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchNonGlobalLiteral("a1b2c3"), ["1"]);
shouldBe(matchNonGlobalLiteral("nope"), null);

// Sticky literal: not strength-reduced to global; goes through RegExpExec.
function matchStickyLiteral(str) {
    return str.match(/[0-9]/y);
}
noInline(matchStickyLiteral);
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchStickyLiteral("1abc"), ["1"]);
shouldBe(matchStickyLiteral("a1bc"), null);

// Unicode global literal exercising surrogate handling.
function matchUnicodeGlobal(str) {
    return str.match(/./gu);
}
noInline(matchUnicodeGlobal);
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchUnicodeGlobal("a\u{1F600}b"), ["a", "\u{1F600}", "b"]);

// Cached RegExp parameter: StringMatch -> RegExpMatchFast (no strength reduction
// since the regexp is not a constant).
function matchCached(str, re) {
    return str.match(re);
}
noInline(matchCached);
var cached = /[0-9]/g;
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchCached("a1b2c3", cached), ["1", "2", "3"]);

// Non-global cached.
var cachedNonGlobal = /[0-9]/;
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchCached("a1b2c3", cachedNonGlobal), ["1"]);

// String pattern: StringMatch with StringUse on child2.
function matchStringPattern(str, pat) {
    return str.match(pat);
}
noInline(matchStringPattern);
for (var i = 0; i < 1e5; ++i)
    shouldBe(matchStringPattern("a1b2c3", "[0-9]"), ["1"]);

// Mixed monomorphic: function that sees both global and non-global regexps.
function matchPoly(str, re) {
    return str.match(re);
}
noInline(matchPoly);
var g = /[0-9]/g;
var ng = /[0-9]/;
for (var i = 0; i < 1e5; ++i) {
    shouldBe(matchPoly("a1b2c3", g), ["1", "2", "3"]);
    shouldBe(matchPoly("a1b2c3", ng), ["1"]);
}

// Result array structure: exec result should be a RegExpMatchesArray.
{
    var r = "ab12".match(/(\d)(\d)/);
    shouldBe(r.length, 3);
    shouldBe(r.index, 2);
    shouldBe(r.input, "ab12");
    shouldBe(r[0], "12");
    shouldBe(r[1], "1");
    shouldBe(r[2], "2");
}

// Legacy properties recorded after a non-global match.
{
    "abc1def".match(/[0-9]/);
    shouldBe(RegExp.lastMatch, "1");
    shouldBe(RegExp.leftContext, "abc");
    shouldBe(RegExp.rightContext, "def");
}
