// ECMA-262 reference implementation for cross-validation.
// https://tc39.es/ecma262/#sec-string.prototype.lastindexof
function specLastIndexOf(S, searchStr, position) {
    var numPos = +position; // ToNumber
    var pos;
    if (numPos !== numPos)
        pos = Infinity;
    else if (numPos === 0 || !isFinite(numPos))
        pos = numPos === -Infinity ? -Infinity : numPos;
    else
        pos = numPos < 0 ? Math.ceil(numPos) : Math.floor(numPos);
    var len = S.length;
    var searchLen = searchStr.length;
    if (len < searchLen)
        return -1;
    // clamp pos to [0, len - searchLen]
    var maxStart = len - searchLen;
    var start;
    if (pos < 0)
        start = 0;
    else if (pos > maxStart)
        start = maxStart;
    else
        start = pos | 0;
    // Scan backward.
    for (var i = start; i >= 0; i--) {
        var match = true;
        for (var j = 0; j < searchLen; j++) {
            if (S.charCodeAt(i + j) !== searchStr.charCodeAt(j)) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return -1;
}

function assertEq(a, b, m) {
    if (a !== b)
        throw new Error(m + ": expected " + b + " got " + a);
}

function check(S, needle, pos, message) {
    var expected = specLastIndexOf(S, needle, pos);
    var actual = pos === undefined ? S.lastIndexOf(needle) : S.lastIndexOf(needle, pos);
    if (actual !== expected)
        throw new Error(message + ": S.lastIndexOf(" + JSON.stringify(needle) + ", " + String(pos) + ") expected " + expected + " got " + actual);
}

// -------- fromIndex edge values --------
var basic = "hello world";
for (var p of [undefined, NaN, Infinity, -Infinity, -0, 0, 0.5, 1, 1.5, 4, 4.7, 6, 7, 10, 11, 12, -1, -100,
               Number.MIN_SAFE_INTEGER, Number.MAX_SAFE_INTEGER,
               1 << 30, -(1 << 30), (1 << 30) * 2, -(1 << 30) * 2]) {
    check(basic, "o", p, "basic/o/" + String(p));
    check(basic, "l", p, "basic/l/" + String(p));
    check(basic, "x", p, "basic/x/" + String(p));
    check(basic, "", p, "basic//" + String(p));
    check(basic, "hello", p, "basic/hello/" + String(p));
    check(basic, "world", p, "basic/world/" + String(p));
    check(basic, "hello world", p, "basic/full/" + String(p));
    check(basic, "hello world!", p, "basic/toolong/" + String(p));
}

// -------- Empty strings --------
check("", "", undefined, "empty/empty");
check("", "", 0, "empty/empty/0");
check("", "", 999, "empty/empty/big");
check("", "x", undefined, "empty/x");
check("abc", "", undefined, "abc/empty");
check("abc", "", 0, "abc/empty/0");
check("abc", "", 1, "abc/empty/1");
check("abc", "", 3, "abc/empty/3");
check("abc", "", 100, "abc/empty/big");

// -------- Non-integer position (ToIntegerOrInfinity truncates toward zero) --------
check("abcdefg", "c", 2.5, "frac/2.5");
check("abcdefg", "c", 2.9, "frac/2.9");
check("abcdefg", "c", -0.5, "frac/-0.5");
check("abcdefg", "c", -0.999, "frac/-0.999");

// -------- Value coercion on receiver and needle --------
assertEq(String.prototype.lastIndexOf.call(123456123, "1"), 6, "number receiver");
assertEq("abc".lastIndexOf(undefined), -1, "undefined needle");
assertEq("abundefinedc".lastIndexOf(undefined), 2, "undefined coerces to 'undefined'");
assertEq("a1b".lastIndexOf(1), 1, "number needle");
assertEq("null".lastIndexOf(null), 0, "null needle");

// position via valueOf
var counted = 0;
var pos = { valueOf: function() { counted++; return 3; } };
assertEq("aaaa".lastIndexOf("a", pos), 3, "valueOf position");
assertEq(counted, 1, "valueOf called once");

// Symbol needle should throw.
try {
    "abc".lastIndexOf(Symbol("s"));
    throw new Error("symbol-needle should throw");
} catch (e) {
    if (!(e instanceof TypeError))
        throw new Error("expected TypeError, got " + e);
}

// -------- SIMD threshold boundary (single-char, 8-bit) --------
for (var n of [0, 1, 2, 15, 16, 17, 31, 32, 33, 47, 48, 49, 63, 64, 100, 255, 256, 257]) {
    var s = "a".repeat(n);
    check(s, "b", undefined, "simd8/nomatch/" + n);
    check(s, "a", undefined, "simd8/allmatch/" + n);
    if (n > 0) {
        // Match at every position.
        for (var k = 0; k < n; k++) {
            var t = "a".repeat(k) + "X" + "a".repeat(n - k - 1);
            check(t, "X", undefined, "simd8/pos/" + n + "/" + k);
            check(t, "X", k, "simd8/pos/" + n + "/" + k + "/k");
            check(t, "X", k - 1, "simd8/pos/" + n + "/" + k + "/k-1");
            check(t, "X", k + 1, "simd8/pos/" + n + "/" + k + "/k+1");
        }
    }
}

// -------- SIMD threshold boundary (single-char, 16-bit) --------
for (var n of [0, 1, 2, 15, 16, 17, 31, 32, 33, 63, 64, 100]) {
    var s16 = "ア".repeat(n);
    check(s16, "イ", undefined, "simd16/nomatch/" + n);
    check(s16, "ア", undefined, "simd16/allmatch/" + n);
    if (n > 0) {
        for (var k of [0, 1, n >> 1, n - 2, n - 1].filter(function(i) { return i >= 0 && i < n; })) {
            var t16 = "ア".repeat(k) + "★" + "ア".repeat(n - k - 1);
            check(t16, "★", undefined, "simd16/pos/" + n + "/" + k);
        }
    }
}

// Non-Latin1 needle in 8-bit haystack must return -1.
assertEq("abc".lastIndexOf("☃"), -1, "non-latin1 in 8bit");
assertEq("☃abc".lastIndexOf("☃"), 0, "non-latin1 in 16bit");

// -------- Rope coverage (length >= minLengthForRopeWalk = 0x128 = 296) --------
// Helper: force a rope by concatenating pieces.
function makeRope2(a, b) {
    return a + b; // JSC typically lazy-concatenates into a rope.
}
function makeRope3(a, b, c) {
    return (a + b) + c;
}

// Rope with single '/' in each of 3 fibers.
var fiberA = "a".repeat(200) + "/" + "b".repeat(50);  // 251 chars, '/' at index 200
var fiberB = "c".repeat(100) + "/" + "d".repeat(50);  // 151 chars, '/' at local 100 (global 351)
var fiberC = "e".repeat(100) + "/" + "f".repeat(50);  // 151 chars, '/' at local 100 (global 502)
var r3 = makeRope3(fiberA, fiberB, fiberC);

check(r3, "/", undefined, "rope3/last");
assertEq(r3.lastIndexOf("/"), 502, "rope3/last/value");
assertEq(r3.lastIndexOf("/", 501), 351, "rope3/middle");
assertEq(r3.lastIndexOf("/", 350), 200, "rope3/first");
assertEq(r3.lastIndexOf("/", 199), -1, "rope3/nothing");
assertEq(r3.lastIndexOf("Z"), -1, "rope3/missing");

// Rope where needle sits at fiber boundary (last char of fiber vs first char of next).
var boundaryA = "x".repeat(150) + "/";  // '/' at 150 (last char)
var boundaryB = "y".repeat(150);
var rBoundary = makeRope2(boundaryA, boundaryB);
assertEq(rBoundary.lastIndexOf("/"), 150, "rope/boundary-last-of-A");
assertEq(rBoundary.lastIndexOf("/", 150), 150, "rope/boundary-exact");
assertEq(rBoundary.lastIndexOf("/", 149), -1, "rope/boundary-before");

var bA2 = "x".repeat(150);
var bB2 = "/" + "y".repeat(150);  // '/' at 150 (first of B, global 150)
var rBoundary2 = makeRope2(bA2, bB2);
assertEq(rBoundary2.lastIndexOf("/"), 150, "rope/boundary-first-of-B");

// Nested rope → bail out through non-substring rope fiber.
var inner = makeRope2("p".repeat(100), "q".repeat(100)); // 200 chars rope
var outer = makeRope2(inner, "/" + "r".repeat(200));      // outer[0] is a non-substring rope
assertEq(outer.lastIndexOf("/"), 200, "nested-rope/last");
// '/' is in fiber[1] at local 0 → global 200. Rope walk scans fiber[1] first (reverse order),
// finds it, returns without bailing.
assertEq(outer.lastIndexOf("/", 199), -1, "nested-rope/before-slash");
// Search for something only in the nested fiber → forces bailout.
var inner2 = makeRope2("a".repeat(150), "@" + "b".repeat(49)); // '@' at local 150 inside inner
var outer2 = makeRope2(inner2, "c".repeat(200));
assertEq(outer2.lastIndexOf("@"), 150, "nested-rope/bailout-find");
assertEq(outer2.lastIndexOf("@", 149), -1, "nested-rope/bailout-miss");
assertEq(outer2.lastIndexOf("@", 150), 150, "nested-rope/bailout-exact");
assertEq(outer2.lastIndexOf("@", 200), 150, "nested-rope/bailout-from-after");

// Substring rope (fiber is a substring of a base).
var base = "_".repeat(100) + "#" + "_".repeat(200); // '#' at 100, length 301
var subrope = base.substring(50, 260);              // length 210, '#' at local 50
// subrope alone may not reach minLengthForRopeWalk; combine.
var rS = makeRope2(subrope, "=".repeat(100));       // length 310
assertEq(rS.lastIndexOf("#"), 50, "substring-rope/find");
assertEq(rS.lastIndexOf("#", 49), -1, "substring-rope/before");
assertEq(rS.lastIndexOf("="), 309, "substring-rope/other-fiber");

// Deep rope stress (many concatenations → multi-level rope tree).
var deep = "";
for (var i = 0; i < 50; i++)
    deep = "0123456789" + deep;
// deep has length 500, contains "/" never.
assertEq(deep.lastIndexOf("/"), -1, "deep/nomatch");
assertEq(deep.lastIndexOf("9"), 499, "deep/last9");
assertEq(deep.lastIndexOf("0", 499), 490, "deep/last0");
assertEq(deep.lastIndexOf("0"), 490, "deep/last0-nopos");

// Rope with startPosition at every fiber boundary.
var p1 = "1".repeat(100);
var p2 = "2".repeat(100);
var p3 = "3".repeat(100);
var pRope = makeRope3(p1, p2, p3);
for (var pos of [0, 50, 99, 100, 101, 150, 199, 200, 201, 250, 299]) {
    check(pRope, "1", pos, "pRope/1/" + pos);
    check(pRope, "2", pos, "pRope/2/" + pos);
    check(pRope, "3", pos, "pRope/3/" + pos);
    check(pRope, "X", pos, "pRope/X/" + pos);
}

// -------- Multi-char needle in rope with spec clamp on long needle --------
var bigRope = makeRope3("abc".repeat(100), "xyz".repeat(100), "abc".repeat(100)); // 900 chars
assertEq(bigRope.lastIndexOf("abc"), 897, "bigRope/abc");
assertEq(bigRope.lastIndexOf("abc", 597), 297, "bigRope/abc/low");
assertEq(bigRope.lastIndexOf("xyz"), 597, "bigRope/xyz");
assertEq(bigRope.lastIndexOf("xyzabc"), 597, "bigRope/xyzabc");
assertEq(bigRope.lastIndexOf("abcabc"), 894, "bigRope/abcabc");

// -------- Empty receiver / needle combinations --------
assertEq("".lastIndexOf("x"), -1, "empty/x");
assertEq("".lastIndexOf(""), 0, "empty/empty/return");
assertEq("x".lastIndexOf(""), 1, "x/empty");

// -------- Spec clamp: long needle --------
assertEq("abc".lastIndexOf("abcdef"), -1, "too-long-needle");
assertEq("abc".lastIndexOf("abc"), 0, "exact-match");
assertEq("abc".lastIndexOf("abc", 0), 0, "exact-match-0");
assertEq("abc".lastIndexOf("abc", 1), 0, "exact-match-clamp");
assertEq("abc".lastIndexOf("abc", 100), 0, "exact-match-bigpos");
assertEq("abc".lastIndexOf("abc", -1), 0, "exact-match-negpos");

// -------- JIT warm-up loop for doxbee-promise pattern --------
function hotSlash(s) { return s.lastIndexOf("/"); }
noInline(hotSlash);

function warmSingleChar() {
    for (var i = 0; i < testLoopCount; i++) {
        assertEq(hotSlash("foo"), -1, "hot/foo");
        assertEq(hotSlash("a/b"), 1, "hot/a/b");
        assertEq(hotSlash("/"), 0, "hot/solo");
        assertEq(hotSlash(""), -1, "hot/empty");
        assertEq(hotSlash("//a"), 1, "hot/double");
    }
}
noInline(warmSingleChar);
warmSingleChar();

// JIT warm-up for the rope fast path.
function hotRope(r) { return r.lastIndexOf("/"); }
noInline(hotRope);

function warmRope() {
    for (var i = 0; i < testLoopCount; i++) {
        assertEq(hotRope(r3), 502, "hot-rope/all");
        assertEq(hotRope(outer2), -1, "hot-rope/bailout-miss");
        assertEq(hotRope(rBoundary), 150, "hot-rope/boundary");
    }
}
noInline(warmRope);
warmRope();

// -------- Middle-fiber bailout: nested rope in the middle of a larger rope --------
// Construct shapes where a non-substring rope fiber sits *between* already-scanned
// higher-offset fibers and unscanned lower-offset fibers, so tryFindLastOneChar must
// scan the tail flat fiber(s), then bail when it hits the nested rope, forcing the
// caller to resolve and finish with reverseFind on the flat result. The exact rope
// layout chosen by JSC is opaque, but the output must still match the reference.
function buildMiddleBailoutRope() {
    var tail = "t".repeat(150);                    // flat, no needle
    var middleInner = "a".repeat(60) + "b".repeat(60); // 120 chars — candidate nested rope
    var head = "h".repeat(100) + "#" + "g".repeat(49); // flat, needle '#' at local 100
    return (head + middleInner) + tail;            // try to keep middle as nested rope
}
var mbr = buildMiddleBailoutRope();
// '#' is at position 100 in head. Total length = 150 + 120 + 150 = 420.
check(mbr, "#", undefined, "middle-bailout/#/any");
check(mbr, "#", 100, "middle-bailout/#/exact");
check(mbr, "#", 99, "middle-bailout/#/before");
check(mbr, "#", 101, "middle-bailout/#/after");
check(mbr, "#", 419, "middle-bailout/#/end");
// Needle only in tail — forces bailout with a non-trivial earlier scan.
var mbr2 = buildMiddleBailoutRope() + "Q" + "z".repeat(100); // 'Q' at position 420
check(mbr2, "Q", undefined, "middle-bailout-append/Q/any");
check(mbr2, "Q", 419, "middle-bailout-append/Q/before");
check(mbr2, "Q", 420, "middle-bailout-append/Q/exact");
// Needle only inside the nested middle — walk scans tail, bails at middle, resolves.
function buildMiddleOnly() {
    var tail = "z".repeat(150);
    var middle = "m".repeat(59) + "@" + "n".repeat(60); // '@' at local 59 inside middle
    var head = "h".repeat(150);
    return (head + middle) + tail;
}
var mo = buildMiddleOnly();
// '@' global position = 150 (head) + 59 = 209.
check(mo, "@", undefined, "middle-only/@/any");
check(mo, "@", 208, "middle-only/@/before");
check(mo, "@", 209, "middle-only/@/exact");
check(mo, "@", 210, "middle-only/@/after");
check(mo, "@", 500, "middle-only/@/past-end");

// -------- Mixed 8-bit / 16-bit rope --------
// Build a rope whose fibers span both encodings. JSC's rope can combine 8-bit and
// 16-bit fibers; search with each encoding of needle must behave correctly.
var f8 = "a".repeat(200) + "X" + "a".repeat(49); // 8-bit, length 250, 'X' at 200
var f16 = "ア".repeat(100) + "★" + "ア".repeat(49); // 16-bit, length 150, '★' at 100
var mixed = f8 + f16; // 8-bit + 16-bit → promoted to 16-bit rope, total 400
assertEq(mixed.length, 400, "mixed/length");
// 'X' (Latin1) appears only in the 8-bit fiber at global index 200.
check(mixed, "X", undefined, "mixed/X/any");
check(mixed, "X", 199, "mixed/X/before");
check(mixed, "X", 200, "mixed/X/exact");
check(mixed, "X", 399, "mixed/X/past");
// '★' (non-Latin1) appears only in the 16-bit fiber at global 250 + 100 = 350.
check(mixed, "★", undefined, "mixed/*/any");
check(mixed, "★", 349, "mixed/*/before");
check(mixed, "★", 350, "mixed/*/exact");
check(mixed, "★", 399, "mixed/*/past");
// 'a' (Latin1) only appears in the 8-bit fiber; last 'a' at global 249.
check(mixed, "a", undefined, "mixed/a/any");
check(mixed, "a", 248, "mixed/a/before");
check(mixed, "a", 249, "mixed/a/exact");
check(mixed, "a", 399, "mixed/a/past");
// Non-Latin1 needle that doesn't appear at all.
check(mixed, "☃", undefined, "mixed/snow/none");
// Latin1 needle that doesn't appear at all.
check(mixed, "Z", undefined, "mixed/Z/none");

// Reverse mixed order: 16-bit first, 8-bit second.
var mixed2 = f16 + f8; // length 400
check(mixed2, "X", undefined, "mixed2/X/any");
check(mixed2, "★", undefined, "mixed2/*/any");
check(mixed2, "a", undefined, "mixed2/a/any");

// Three-fiber mixed rope.
var mixed3 = (f8 + f16) + f8; // 250 + 150 + 250 = 650
check(mixed3, "X", undefined, "mixed3/X/any");
check(mixed3, "★", undefined, "mixed3/*/any");
check(mixed3, "a", undefined, "mixed3/a/any");

// JIT warm-up for middle-bailout and mixed-encoding paths.
function hotMiddle(r) { return r.lastIndexOf("@"); }
noInline(hotMiddle);
function hotMixed8(r) { return r.lastIndexOf("X"); }
noInline(hotMixed8);
function hotMixed16(r) { return r.lastIndexOf("★"); }
noInline(hotMixed16);

function warmMiddleMixed() {
    var expectedMiddle = mo.lastIndexOf("@");
    var expectedMixed8 = mixed.lastIndexOf("X");
    var expectedMixed16 = mixed.lastIndexOf("★");
    for (var i = 0; i < testLoopCount; i++) {
        assertEq(hotMiddle(mo), expectedMiddle, "hot-middle");
        assertEq(hotMixed8(mixed), expectedMixed8, "hot-mixed8");
        assertEq(hotMixed16(mixed), expectedMixed16, "hot-mixed16");
    }
}
noInline(warmMiddleMixed);
warmMiddleMixed();
