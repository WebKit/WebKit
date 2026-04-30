// Tests for String.prototype.indexOf / lastIndexOf when base or search is empty.
// Cross-validates against ECMA-262 reference implementations across multiple
// string shapes (8-bit, 16-bit, rope, substring-of-rope, mixed-encoding rope).
//
// https://tc39.es/ecma262/#sec-string.prototype.indexof
// https://tc39.es/ecma262/#sec-string.prototype.lastindexof

function assertEq(a, b, m) {
    if (a !== b)
        throw new Error(m + ": expected " + b + " got " + a);
}

function specIndexOf(S, searchStr, position) {
    // position undefined -> ToNumber(undefined) = NaN -> ToIntegerOrInfinity(NaN) = 0.
    var numPos = +position;
    var pos;
    if (numPos !== numPos)
        pos = 0;
    else if (!isFinite(numPos))
        pos = numPos;
    else
        pos = numPos < 0 ? Math.ceil(numPos) : Math.floor(numPos);
    var len = S.length;
    var searchLen = searchStr.length;
    var start;
    if (pos < 0)
        start = 0;
    else if (pos > len)
        start = len;
    else
        start = pos;
    // Empty search: spec's StringIndexOf returns min(start, len) = start.
    if (searchLen === 0)
        return start;
    for (var i = start; i + searchLen <= len; i++) {
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

function specLastIndexOf(S, searchStr, position) {
    var numPos = +position;
    var pos;
    if (numPos !== numPos)
        pos = Infinity; // NaN -> +Infinity for lastIndexOf.
    else if (numPos === 0 || !isFinite(numPos))
        pos = numPos === -Infinity ? -Infinity : numPos;
    else
        pos = numPos < 0 ? Math.ceil(numPos) : Math.floor(numPos);
    var len = S.length;
    var searchLen = searchStr.length;
    if (len < searchLen)
        return -1;
    var maxStart = len - searchLen;
    var start;
    if (pos < 0)
        start = 0;
    else if (pos > maxStart)
        start = maxStart;
    else
        start = pos | 0;
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

function checkIndex(S, needle, pos, tag) {
    var expected = specIndexOf(S, needle, pos);
    var actual = pos === undefined ? S.indexOf(needle) : S.indexOf(needle, pos);
    if (actual !== expected)
        throw new Error("indexOf " + tag + ": S.indexOf(" + JSON.stringify(needle) + ", " + String(pos) + ") expected " + expected + " got " + actual);
}

function checkLast(S, needle, pos, tag) {
    var expected = specLastIndexOf(S, needle, pos);
    var actual = pos === undefined ? S.lastIndexOf(needle) : S.lastIndexOf(needle, pos);
    if (actual !== expected)
        throw new Error("lastIndexOf " + tag + ": S.lastIndexOf(" + JSON.stringify(needle) + ", " + String(pos) + ") expected " + expected + " got " + actual);
}

function checkBoth(S, needle, pos, tag) {
    checkIndex(S, needle, pos, tag);
    checkLast(S, needle, pos, tag);
}

// All fromIndex values we iterate over for each shape.
var positions = [
    undefined, NaN, Infinity, -Infinity,
    -0, 0, 1,
    -1, -2, -100, Number.MIN_SAFE_INTEGER,
    100, (1 << 30), Number.MAX_SAFE_INTEGER,
    0.5, 1.5, -0.5, -1.5, 1e20, -1e20,
];

function exerciseEmptyNeedle(S, tag) {
    for (var p of positions)
        checkBoth(S, "", p, tag + "/empty-needle/" + String(p));
    // At every position in [0, len].
    for (var i = 0; i <= S.length; i++)
        checkBoth(S, "", i, tag + "/empty-needle/pos-" + i);
}

function exerciseNonEmptyNeedleOnEmpty(S, tag) {
    // S is empty; every non-empty needle must miss.
    for (var p of positions) {
        checkBoth(S, "x", p, tag + "/x/" + String(p));
        checkBoth(S, "xyz", p, tag + "/xyz/" + String(p));
        checkBoth(S, "ア", p, tag + "/16bit/" + String(p));
    }
}

// -------- 1. Both empty --------
checkBoth("", "", undefined, "both-empty/default");
for (var p of positions)
    checkBoth("", "", p, "both-empty/" + String(p));

// -------- 2. Empty base, non-empty needle --------
exerciseNonEmptyNeedleOnEmpty("", "empty-base");

// -------- 3. Non-empty base (8-bit), empty needle --------
exerciseEmptyNeedle("a", "len1-8bit");
exerciseEmptyNeedle("ab", "len2-8bit");
exerciseEmptyNeedle("hello world", "basic-8bit");
exerciseEmptyNeedle("x".repeat(257), "long-8bit"); // crosses SIMD thresholds.

// -------- 4. Non-empty base (16-bit), empty needle --------
exerciseEmptyNeedle("ア", "len1-16bit");
exerciseEmptyNeedle("アイ", "len2-16bit");
exerciseEmptyNeedle("ab☃cd", "short-16bit");
exerciseEmptyNeedle("ア".repeat(257), "long-16bit");

// -------- 5. Rope base (>= minLengthForRopeWalk = 296), empty needle --------
var rope8 = "a".repeat(200) + "b".repeat(200);
exerciseEmptyNeedle(rope8, "rope-8bit");

var rope16 = "ア".repeat(200) + "イ".repeat(200);
exerciseEmptyNeedle(rope16, "rope-16bit");

// -------- 6. Mixed-encoding rope, empty needle --------
var mixedRope = "a".repeat(200) + "ア".repeat(200);
exerciseEmptyNeedle(mixedRope, "rope-mixed-8-16");

var mixedRope2 = "ア".repeat(200) + "a".repeat(200);
exerciseEmptyNeedle(mixedRope2, "rope-mixed-16-8");

// -------- 7. Substring rope, empty needle --------
var baseStr = "_".repeat(100) + "#" + "_".repeat(500);
var subRope = baseStr.substring(50, 550) + "=".repeat(100); // length 600; exercises substring fiber.
exerciseEmptyNeedle(subRope, "substring-rope");

// -------- 8. Empty needle with non-empty base, non-empty needle absent --------
// Sanity: empty needle at every position should always be min(start, len); non-empty absent needle returns -1.
var nonEmpty = "hello world";
for (var p of positions) {
    checkIndex(nonEmpty, "", p, "sanity/empty-present/" + String(p));
    checkLast(nonEmpty, "", p, "sanity/empty-present/" + String(p));
    checkIndex(nonEmpty, "Z", p, "sanity/nonempty-absent/" + String(p));
    checkLast(nonEmpty, "Z", p, "sanity/nonempty-absent/" + String(p));
}

// -------- 9. Empty base with position (edge clamp) --------
checkBoth("", "", undefined, "empty/empty/default");
checkBoth("", "", 0, "empty/empty/0");
checkBoth("", "", 1, "empty/empty/1");
checkBoth("", "", -1, "empty/empty/-1");
checkBoth("", "", Infinity, "empty/empty/inf");
checkBoth("", "x", 0, "empty/x/0");
checkBoth("", "x", Infinity, "empty/x/inf");

// -------- 10. Empty needle at the boundary around string length --------
for (var len of [0, 1, 2, 15, 16, 17, 31, 32, 33, 255, 256, 257]) {
    var s = "a".repeat(len);
    for (var p of [-1, 0, 1, len - 1, len, len + 1, len * 2]) {
        checkIndex(s, "", p, "boundary/len" + len + "/indexOf/" + p);
        checkLast(s, "", p, "boundary/len" + len + "/lastIndexOf/" + p);
    }
}

// -------- JIT warm-up: drive through all tiers --------
function idxEmpty(s) { return s.indexOf(""); }
noInline(idxEmpty);
function lastEmpty(s) { return s.lastIndexOf(""); }
noInline(lastEmpty);
function idxEmptyAt(s, p) { return s.indexOf("", p); }
noInline(idxEmptyAt);
function lastEmptyAt(s, p) { return s.lastIndexOf("", p); }
noInline(lastEmptyAt);
function idxOnEmpty(needle) { return "".indexOf(needle); }
noInline(idxOnEmpty);
function lastOnEmpty(needle) { return "".lastIndexOf(needle); }
noInline(lastOnEmpty);

function warmEmpty() {
    for (var i = 0; i < testLoopCount; i++) {
        // empty needle on non-empty base
        assertEq(idxEmpty("hello"), 0, "warm/idx/hello");
        assertEq(idxEmpty(""), 0, "warm/idx/empty");
        assertEq(lastEmpty("hello"), 5, "warm/last/hello");
        assertEq(lastEmpty(""), 0, "warm/last/empty");

        // empty needle at a given position
        assertEq(idxEmptyAt("hello", 3), 3, "warm/idxAt/3");
        assertEq(idxEmptyAt("hello", 100), 5, "warm/idxAt/big");
        assertEq(idxEmptyAt("hello", -1), 0, "warm/idxAt/neg");
        assertEq(lastEmptyAt("hello", 3), 3, "warm/lastAt/3");
        assertEq(lastEmptyAt("hello", 100), 5, "warm/lastAt/big");
        assertEq(lastEmptyAt("hello", -1), 0, "warm/lastAt/neg");

        // non-empty needle on empty base
        assertEq(idxOnEmpty("x"), -1, "warm/idxEmpty/x");
        assertEq(idxOnEmpty(""), 0, "warm/idxEmpty/empty");
        assertEq(lastOnEmpty("x"), -1, "warm/lastEmpty/x");
        assertEq(lastOnEmpty(""), 0, "warm/lastEmpty/empty");
    }
}
noInline(warmEmpty);
warmEmpty();

// Warm up with a 16-bit base too.
function idxEmptyU(s) { return s.indexOf(""); }
noInline(idxEmptyU);
function lastEmptyU(s) { return s.lastIndexOf(""); }
noInline(lastEmptyU);

function warmEmptyU() {
    var u = "アイ☃";
    for (var i = 0; i < testLoopCount; i++) {
        assertEq(idxEmptyU(u), 0, "warmU/idx");
        assertEq(lastEmptyU(u), 3, "warmU/last");
    }
}
noInline(warmEmptyU);
warmEmptyU();

// Warm up with a rope base so the rope fast paths see empty-needle traffic.
function idxEmptyRope(s) { return s.indexOf(""); }
noInline(idxEmptyRope);
function lastEmptyRope(s) { return s.lastIndexOf(""); }
noInline(lastEmptyRope);

function warmEmptyRope() {
    for (var i = 0; i < testLoopCount; i++) {
        assertEq(idxEmptyRope(rope8), 0, "warmR/idx/8");
        assertEq(lastEmptyRope(rope8), rope8.length, "warmR/last/8");
        assertEq(idxEmptyRope(rope16), 0, "warmR/idx/16");
        assertEq(lastEmptyRope(rope16), rope16.length, "warmR/last/16");
        assertEq(idxEmptyRope(mixedRope), 0, "warmR/idx/mixed");
        assertEq(lastEmptyRope(mixedRope), mixedRope.length, "warmR/last/mixed");
    }
}
noInline(warmEmptyRope);
warmEmptyRope();
