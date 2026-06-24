// Exercises the DFG path where a StringSearch node is converted to RegExpSearch
// in fixup and then further strength-reduced for known-flag literal RegExp
// arguments.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

// Non-global literal: StringSearch -> RegExpSearch (strength-reduced for the literal).
function searchLiteral(str) {
    return str.search(/[0-9]/);
}
noInline(searchLiteral);
for (var i = 0; i < 1e5; ++i)
    shouldBe(searchLiteral("a1b2c3"), 1);
shouldBe(searchLiteral("nope"), -1);

// Global literal: search ignores the global flag and always starts from index 0.
function searchGlobalLiteral(str) {
    return str.search(/[0-9]/g);
}
noInline(searchGlobalLiteral);
for (var i = 0; i < 1e5; ++i)
    shouldBe(searchGlobalLiteral("a1b2c3"), 1);
shouldBe(searchGlobalLiteral("nope"), -1);

// Sticky literal: @@search resets lastIndex to 0, so sticky only matches at the start.
function searchStickyLiteral(str) {
    return str.search(/[0-9]/y);
}
noInline(searchStickyLiteral);
for (var i = 0; i < 1e5; ++i)
    shouldBe(searchStickyLiteral("1abc"), 0);
shouldBe(searchStickyLiteral("a1bc"), -1);

// Unicode literal exercising surrogate handling.
function searchUnicode(str) {
    return str.search(/\u{1F600}/u);
}
noInline(searchUnicode);
for (var i = 0; i < 1e5; ++i)
    shouldBe(searchUnicode("a\u{1F600}b"), 1);

// Cached RegExp parameter: StringSearch -> RegExpSearch (no strength reduction
// since the regexp is not a constant).
function searchCached(str, re) {
    return str.search(re);
}
noInline(searchCached);
var cached = /[0-9]/;
for (var i = 0; i < 1e5; ++i)
    shouldBe(searchCached("a1b2c3", cached), 1);

// Cached global RegExp with non-zero lastIndex: search always starts from 0 and
// must leave lastIndex untouched on the fast path.
var cachedGlobal = /[0-9]/g;
cachedGlobal.lastIndex = 5;
for (var i = 0; i < 1e5; ++i) {
    shouldBe(searchCached("a1b2c3", cachedGlobal), 1);
    shouldBe(cachedGlobal.lastIndex, 5);
}

// String pattern: StringSearch with StringUse on child2.
function searchStringPattern(str, pat) {
    return str.search(pat);
}
noInline(searchStringPattern);
for (var i = 0; i < 1e5; ++i)
    shouldBe(searchStringPattern("a1b2c3", "[0-9]"), 1);

// Mixed: function that sees both global and non-global regexps.
function searchPoly(str, re) {
    return str.search(re);
}
noInline(searchPoly);
var g = /[0-9]/g;
var ng = /[0-9]/;
for (var i = 0; i < 1e5; ++i) {
    shouldBe(searchPoly("a1b2c3", g), 1);
    shouldBe(searchPoly("a1b2c3", ng), 1);
}

// Legacy properties recorded after a search.
{
    "abc1def".search(/[0-9]/);
    shouldBe(RegExp.lastMatch, "1");
    shouldBe(RegExp.leftContext, "abc");
    shouldBe(RegExp.rightContext, "def");
}
