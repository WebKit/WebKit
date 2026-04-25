function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg ? msg + ": " : "") + "got " + actual + ", expected " + expected);
}

function makeRope(unit, depth) {
    // depth-level balanced ternary rope, each fiber is `unit`
    let s = unit;
    for (let i = 0; i < depth; i++)
        s = s + s + s;
    return s;
}

function flatten(s) { s.charCodeAt(0); return s; }

// Shallow 2-fiber rope with resolved fibers: tryFindOneChar walks without bailout.
// Length = 1500 + 1500 = 3000 >= minLengthForRopeWalk (0x128 = 296)
let shallowLeft = flatten("a".repeat(500) + "b" + "a".repeat(499) + "c" + "a".repeat(499));
let shallowRight = flatten("a".repeat(499) + "d" + "a".repeat(500) + "e" + "a".repeat(499));
function mkShallowRope() { return shallowLeft + shallowRight; }
let shallowRope = mkShallowRope();
let shallowFlat = ("" + shallowRope).slice(0); // force resolve

// Deep rope: nested rope fibers → tryFindOneChar bails out (nullopt), falls through to view()
let deepRope = makeRope("a".repeat(100) + "X" + "a".repeat(99), 4); // depth 4, len = 200 * 81 = 16200
let deepFlat = ("" + deepRope).slice(0);

// 16-bit 2-fiber rope with resolved fibers
let wideLeft = flatten("あ".repeat(500) + "漢" + "あ".repeat(499));
let wideRight = flatten("あ".repeat(500) + "字" + "あ".repeat(499));
function mkWideRope() { return wideLeft + wideRight; }
let wideRope = mkWideRope();
let wideFlat = ("" + wideRope).slice(0);

// needle must be a variable so DFG/FTL can't fold to WithOneChar at compile time.
// Also pass through a function parameter to defeat tryGetString.
function indexOf2(s, needle) { return s.indexOf(needle); }
function indexOf3(s, needle, start) { return s.indexOf(needle, start); }
noInline(indexOf2);
noInline(indexOf3);

let needles1 = ["a", "b", "c", "d", "e", "X", "z", ""];
let needles2 = ["ab", "ba", "aa"];
let needlesW = ["あ", "漢", "字"];

for (let i = 0; i < testLoopCount; i++) {
    // shallow rope, 1-char needle: hit/miss
    for (let n of needles1) {
        shouldBe(indexOf2(shallowRope, n), shallowFlat.indexOf(n), "shallow/" + n);
        shouldBe(indexOf2(shallowRope, n), indexOf2(shallowFlat, n), "shallow-cross/" + n);
    }
    // shallow rope, 1-char needle with start index: before/at/after first occurrence, fiber boundary, past end
    for (let start of [0, 499, 500, 501, 1000, 1001, 1499, 1500, 1501, 1999, 2000, 2999, 3000, 9999, -5]) {
        for (let n of ["b", "c", "d", "e", "z"]) {
            shouldBe(indexOf3(shallowRope, n, start), shallowFlat.indexOf(n, start), "shallow-idx/" + n + "/" + start);
        }
    }
    // 0-char / 2-char needle: fall-through path, must still be correct
    for (let n of needles2) {
        shouldBe(indexOf2(shallowRope, n), shallowFlat.indexOf(n), "shallow-multi/" + n);
        shouldBe(indexOf3(shallowRope, n, 100), shallowFlat.indexOf(n, 100), "shallow-multi-idx/" + n);
    }
    // deep rope: bail-out path, pos advanced then fall-through
    for (let n of ["X", "a", "z"]) {
        shouldBe(indexOf2(deepRope, n), deepFlat.indexOf(n), "deep/" + n);
        shouldBe(indexOf3(deepRope, n, 5000), deepFlat.indexOf(n, 5000), "deep-idx/" + n);
    }
    // 16-bit
    for (let n of needlesW) {
        shouldBe(indexOf2(wideRope, n), wideFlat.indexOf(n), "wide/" + n);
        shouldBe(indexOf3(wideRope, n, 600), wideFlat.indexOf(n, 600), "wide-idx/" + n);
    }
    // flat base: result unchanged
    shouldBe(indexOf2(shallowFlat, "b"), 500, "flat-b");
    shouldBe(indexOf3(shallowFlat, "c", 600), 1000, "flat-c-idx");
}

// Fiber-boundary characters: last/first char of each resolved fiber.
let boundLeft = flatten("a".repeat(1499) + "P");
let boundRight = flatten("Q" + "a".repeat(1499));
function mkBoundRope() { return boundLeft + boundRight; }
let boundFlat = flatten(mkBoundRope());
for (let i = 0; i < testLoopCount; i++) {
    let boundRope = mkBoundRope();
    shouldBe(indexOf2(boundRope, "P"), 1499, "bound-P");
    shouldBe(indexOf2(boundRope, "Q"), 1500, "bound-Q");
    shouldBe(indexOf3(boundRope, "P", 1499), 1499, "bound-P-at");
    shouldBe(indexOf3(boundRope, "P", 1500), -1, "bound-P-past");
    shouldBe(indexOf3(boundRope, "Q", 1500), 1500, "bound-Q-at");
    shouldBe(indexOf2(boundRope, "P"), boundFlat.indexOf("P"), "bound-cross-P");
    shouldBe(indexOf2(boundRope, "Q"), boundFlat.indexOf("Q"), "bound-cross-Q");
}

// Below minLengthForRopeWalk: short rope skips the optimization, falls through
let shortRope = ("a".repeat(50) + "M") + ("a".repeat(50)) + ("a".repeat(50));
let shortFlat = ("" + shortRope).slice(0);
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(indexOf2(shortRope, "M"), shortFlat.indexOf("M"), "short-M");
    shouldBe(indexOf3(shortRope, "M", 40), shortFlat.indexOf("M", 40), "short-M-idx");
}
