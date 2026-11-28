function lt(a, b) { return a < b; }
function le(a, b) { return a <= b; }
function gt(a, b) { return a > b; }
function ge(a, b) { return a >= b; }
noInline(lt); noInline(le); noInline(gt); noInline(ge);
var o1 = {}, o2 = {};
var values = [o1, o2, [], [1], [2, 3], "", "10", "9", "abc", "[object Object]", 1, 10, 9, -0, NaN, 1.5, true, null, undefined, 5n, {valueOf() { return 3; }}, {toString() { return "9"; }}, new Date(0)];
var expected = [];
for (var a of values) for (var b of values) expected.push([a < b, a <= b, a > b, a >= b]);
for (var iter = 0; iter < testLoopCount / 40; ++iter) {
    var k = 0;
    for (var a of values) for (var b of values) {
        var e = expected[k++];
        if (lt(a, b) !== e[0] || le(a, b) !== e[1] || gt(a, b) !== e[2] || ge(a, b) !== e[3])
            throw new Error("mismatch " + String(a) + " " + String(b));
    }
}

var prefix = "a".repeat(20);
var flatAA = "aaaaaaaaaaaaaaaaaaaaa";
var flatAB = "aaaaaaaaaaaaaaaaaaaab";
if (flatAA !== prefix + "a" || flatAB !== prefix + "b")
    throw new Error("flat strings");
function makeRope(suffix) { return prefix + suffix; }
noInline(makeRope);

var equal = [false, true, false, true];
var less = [true, true, false, false];
var greater = [false, false, true, true];
var comparisons = [lt, le, gt, ge];
// Each comparison gets fresh operands, since comparing a rope resolves it.
function shouldCompare(name, makeOperands, expected) {
    for (var i = 0; i < comparisons.length; ++i) {
        var [a, b] = makeOperands();
        if (comparisons[i](a, b) !== expected[i])
            throw new Error(name + " " + comparisons[i].name);
    }
}
for (var iter = 0; iter < testLoopCount / 40; ++iter) {
    shouldCompare("same rope", () => { var r = makeRope("a"); return [r, r]; }, equal);
    shouldCompare("rope and equal flat", () => [makeRope("a"), flatAA], equal);
    shouldCompare("flat and equal rope", () => [flatAA, makeRope("a")], equal);
    shouldCompare("equal ropes", () => [makeRope("b"), makeRope("b")], equal);
    shouldCompare("rope and greater flat", () => [makeRope("a"), flatAB], less);
    shouldCompare("flat and smaller rope", () => [flatAB, makeRope("a")], greater);
    shouldCompare("smaller and greater ropes", () => [makeRope("a"), makeRope("b")], less);
    shouldCompare("greater and smaller ropes", () => [makeRope("b"), makeRope("a")], greater);
    shouldCompare("object returning the same rope", () => { var r = makeRope("a"); return [{ valueOf() { return r; } }, r]; }, equal);
    shouldCompare("object returning a greater rope", () => [{ toString() { return makeRope("b"); } }, makeRope("a")], greater);
}
