function eq(a, b) { return a == b; }
function neq(a, b) { return a != b; }
function jeq(a, b) { if (a == b) return 1; return 0; }
function jneq(a, b) { if (a != b) return 1; return 0; }
function eqBaseline(a, b) { return a == b; }
function neqBaseline(a, b) { return a != b; }
function jeqBaseline(a, b) { if (a == b) return 1; return 0; }
function jneqBaseline(a, b) { if (a != b) return 1; return 0; }
function eqFalse(a) { if (a == false) return 1; return 0; }
function neqTrue(a) { return a != true; }
noInline(eq); noInline(neq); noInline(jeq); noInline(jneq); noInline(eqFalse); noInline(neqTrue);
noInline(eqBaseline); noInline(neqBaseline); noInline(jeqBaseline); noInline(jneqBaseline);
noDFG(eqBaseline); noDFG(neqBaseline); noDFG(jeqBaseline); noDFG(jneqBaseline);
var values = [true, false, 0, 1, -0, 0.5, NaN, "", "1", "true", null, undefined, {}, [], Symbol.iterator, 1n, 0n, {valueOf() { return 1; }}, new Boolean(false)];
// [value == false, value == true] for each entry of values, written out by hand so that the
// expectations do not come from the boolean fast paths under test.
var looseEqualsBoolean = [
    [false, true], [true, false], [true, false], [false, true], [true, false], [false, false], [false, false], [true, false],
    [false, true], [false, false], [false, false], [false, false], [false, false], [true, false], [false, false], [false, true],
    [true, false], [false, true], [true, false],
];
if (looseEqualsBoolean.length !== values.length)
    throw new Error("expectation table size");
var expected = [];
for (var i = 0; i < values.length; ++i) {
    for (var j = 0; j < values.length; ++j) {
        var a = values[i];
        var b = values[j];
        var r;
        if (typeof a === "boolean")
            r = looseEqualsBoolean[j][a ? 1 : 0];
        else if (typeof b === "boolean")
            r = looseEqualsBoolean[i][b ? 1 : 0];
        else {
            try { r = a == b; } catch (e) { r = "throw"; }
        }
        expected.push(r);
    }
}
function check(name, actual, expect) { if (actual !== expect) throw new Error(name + " mismatch " + actual + " " + expect); }
for (var iter = 0; iter < testLoopCount / 40; ++iter) {
    var k = 0;
    for (var a of values) for (var b of values) {
        var e = expected[k++];
        if (e === "throw") continue;
        check("eq", eq(a, b), e);
        check("neq", neq(a, b), !e);
        check("jeq", jeq(a, b), e ? 1 : 0);
        check("jneq", jneq(a, b), e ? 0 : 1);
        check("eqBaseline", eqBaseline(a, b), e);
        check("neqBaseline", neqBaseline(a, b), !e);
        check("jeqBaseline", jeqBaseline(a, b), e ? 1 : 0);
        check("jneqBaseline", jneqBaseline(a, b), e ? 0 : 1);
    }
    for (var i = 0; i < values.length; ++i) {
        var [isFalse, isTrue] = looseEqualsBoolean[i];
        check("eqFalse", eqFalse(values[i]), isFalse ? 1 : 0);
        check("neqTrue", neqTrue(values[i]), !isTrue);
    }
}
