// A /u or /v match start position is never placed inside a surrogate pair.
//
// RegExpBuiltinExec (ES2025 22.2.7.2) step 11 retries a failed match attempt at
// AdvanceStringIndex(S, lastIndex, true) (22.2.7.3), which steps over a whole
// code point. So for a pattern that can match emptily, a position between a
// leading and a trailing surrogate is never tried, and a match there is not
// reported. The same advance governs RegExp.prototype[@@split]'s q and the
// empty-match advance in [@@match] / [@@replace] / [@@matchAll].
//
// Section B is the counterpart constraint: an explicitly assigned lastIndex
// inside a pair IS a legal start position, because the caller supplies it
// rather than AdvanceStringIndex producing it. Sections C and F are unaffected
// cases that must keep working.
//
// Expectations here are the spec's, cross-checked against V8. This test should
// pass with both --useRegExpJIT=true and --useRegExpJIT=false.

var failures = [];

function check(label, actual, expected)
{
    var a = JSON.stringify(actual);
    var e = JSON.stringify(expected);
    if (a !== e)
        failures.push(label + ": expected " + e + " but got " + a);
}

function execResult(re, string)
{
    var match = re.exec(string);
    return match === null ? null : [match.index, Array.prototype.slice.call(match)];
}

// [X][D800+DE00][Z]: a lone position at index 2, in the middle of the pair.
var pair = "X\u{1F600}Z";
// Two adjacent pairs, so the interior positions are 1 and 3.
var pairs = "\u{1F600}\u{1F600}";

// A. A failed attempt skips the whole code point, so index 2 is never tried.
check("/\\B/u.exec", execResult(/\B/u, pair), null);
check("/\\B/v.exec", execResult(new RegExp("\\B", "v"), pair), null);
check("/aa|\\B/u.exec", execResult(/aa|\B/u, pair), null);
check("/a?\\B|q/u.exec", execResult(/a?\B|q/u, pair), null);
check("/[\\u{1F600}]X|\\B/u.exec", execResult(/[\u{1F600}]X|\B/u, pair), null);
check("/\\B|[\\u{1F600}]X/u.exec", execResult(/\B|[\u{1F600}]X/u, pair), null);
check("/(?<=\\uD83D)/u.exec", execResult(/(?<=\uD83D)/u, pair), null);
check("/\\B/u.exec adjacent pairs", execResult(/\B/u, pairs), [0, [""]]);

// The first start position that is not interior to a pair still matches.
check("/aa|(?=Z)/u.exec", execResult(/aa|(?=Z)/u, pair), [3, [""]]);
check("/x+|q/u.exec", execResult(/x+|q/u, "\u{1F600}\u{1F600}q"), [4, ["q"]]);

// B. A lastIndex the caller supplies is used as given, pair interior or not.
function stickyAt(flags, index)
{
    var re = new RegExp("\\B", flags);
    re.lastIndex = index;
    var match = re.exec(pair);
    return [match === null ? null : match.index, re.lastIndex];
}
check("/\\B/uy lastIndex=2", stickyAt("uy", 2), [2, 2]);
check("/\\B/ug lastIndex=2", stickyAt("ug", 2), [2, 2]);
check("/\\B/uy lastIndex=1", stickyAt("uy", 1), [null, 0]);
check("/\\B/uy lastIndex=3", stickyAt("uy", 3), [null, 0]);

// C. A failed non-global, non-sticky exec leaves lastIndex alone.
check("/\\B/u lastIndex after failure", (function () {
    var re = /\B/u;
    re.lastIndex = 0;
    re.exec(pair);
    return re.lastIndex;
})(), 0);

// D. Repeated matching walks code points, so no empty match lands inside a pair.
check("match(/\\B/gu)", pair.match(/\B/gu), null);
check("matchAll(/\\B/gu) indices", Array.from(pair.matchAll(/\B/gu), function (m) { return m.index; }), []);
check("match(/\\B/g) is unaffected", pair.match(/\B/g), [""]);
// The empty pattern matches at every position it is tried at, which shows the
// advance in isolation: 0, 1 (start of the pair), 3, 4 --- never 2.
check("matchAll(/(?:)/gu) indices", Array.from(pair.matchAll(/(?:)/gu), function (m) { return m.index; }), [0, 1, 3, 4]);
check("match(/(?:)/gu)", pair.match(/(?:)/gu), ["", "", "", ""]);
check("match(/(?:)/g) is unaffected", pair.match(/(?:)/g), ["", "", "", "", ""]);
check("match(/(?:)/gu) adjacent pairs", pairs.match(/(?:)/gu), ["", "", ""]);

// E. replace and split inherit the same advance.
check("replace(/\\B/gu)", pair.replace(/\B/gu, "-"), "X\u{1F600}Z");
check("replace(/(?:)/gu)", pair.replace(/(?:)/gu, "-"), "-X-\u{1F600}-Z-");
check("split(/\\B/u)", pair.split(/\B/u), ["X\u{1F600}Z"]);
check("split(/\\B/gu)", pair.split(/\B/gu), ["X\u{1F600}Z"]);
check("split(/(?:)/u)", pair.split(/(?:)/u), ["X", "\u{1F600}", "Z"]);

// F. Patterns that consume a character are unaffected: a read at a pair's
// trailing surrogate yields no code point, so those attempts already fail.
check("match(/./gu)", pair.match(/./gu), ["X", "\u{1F600}", "Z"]);
check("match(/[^q]/gu)", pair.match(/[^q]/gu), ["X", "\u{1F600}", "Z"]);
check("/\\uDE00/u.exec", execResult(/\uDE00/u, pair), null);
check("/\\uDE00/uy lastIndex=2", (function () {
    var re = /\uDE00/uy;
    re.lastIndex = 2;
    return execResult(re, pair);
})(), null);
check("/\\uDC00/u.exec lone trailing surrogate", execResult(/\uDC00/u, "\u{10000}\uDC00"), [2, ["\uDC00"]]);
check("/.x/u.exec", execResult(/.x/u, "a\u{10000}x"), [1, ["\u{10000}x"]]);

if (failures.length) {
    for (var i = 0; i < failures.length; ++i)
        print(failures[i]);
    throw new Error(failures.length + " of the AdvanceStringIndex expectations failed");
}
