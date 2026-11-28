function jeqFalse(a) { if (a == false) return 1; return 0; }
function jeqTrue(a) { if (a == true) return 1; return 0; }
function jneqFalse(a) { if (a != false) return 1; return 0; }
function jneqTrue(a) { if (a != true) return 1; return 0; }
function jeqFalseLeft(a) { if (false == a) return 1; return 0; }
function jneqTrueLeft(a) { if (true != a) return 1; return 0; }
function eqFalse(a) { return a == false; }
function eqTrue(a) { return a == true; }
function neqFalse(a) { return a != false; }
function neqTrue(a) { return a != true; }
function eqTrueLeft(a) { return true == a; }
function neqFalseLeft(a) { return false != a; }

var functions = [jeqFalse, jeqTrue, jneqFalse, jneqTrue, jeqFalseLeft, jneqTrueLeft, eqFalse, eqTrue, neqFalse, neqTrue, eqTrueLeft, neqFalseLeft];
for (var f of functions)
    noInline(f);

var values = [true, false, 0, 1, 2, -0, 0.5, NaN, "", "0", "1", "true", null, undefined, {}, [], [0], [1], 1n, 0n, { valueOf() { return 1; } }, { valueOf() { return 0; } }, new Boolean(false), new Boolean(true), Symbol.iterator];
var looseEqualsBoolean = [
    [false, true], [true, false], [true, false], [false, true], [false, false], [true, false], [false, false], [false, false],
    [true, false], [true, false], [false, true], [false, false], [false, false], [false, false], [false, false], [true, false],
    [true, false], [false, true], [false, true], [true, false], [false, true], [true, false], [true, false], [false, true],
    [false, false],
];
if (looseEqualsBoolean.length !== values.length)
    throw new Error("expectation table size");
var expected = [];
for (var [isFalse, isTrue] of looseEqualsBoolean) {
    expected.push([
        isFalse ? 1 : 0, isTrue ? 1 : 0, isFalse ? 0 : 1, isTrue ? 0 : 1,
        isFalse ? 1 : 0, isTrue ? 0 : 1,
        isFalse, isTrue, !isFalse, !isTrue, isTrue, !isFalse,
    ]);
}

for (var iter = 0; iter < testLoopCount; ++iter) {
    for (var i = 0; i < functions.length; ++i) {
        var f = functions[i];
        if (iter & 1) {
            for (var j = 0; j < values.length; ++j) {
                var result = f(values[j]);
                if (result !== expected[j][i])
                    throw new Error(f.name + "(" + String(values[j]) + ") returned " + result);
            }
        } else if (f(true) !== expected[0][i] || f(false) !== expected[1][i])
            throw new Error(f.name + " on a boolean");
    }
}

var calls = 0;
var object = { valueOf() { ++calls; return 1; } };
for (var i = 0; i < testLoopCount; ++i) {
    if (!eqTrue(object) || jeqFalse(object))
        throw new Error("object with valueOf");
}
if (calls !== testLoopCount * 2)
    throw new Error("valueOf called " + calls + " times");

function constEqTrueFalse() { return true == false; }
function constEqTrueTrue() { return true == true; }
function constNeqFalseTrue() { return false != true; }
function constNeqFalseFalse() { return false != false; }
function constJeqTrueFalse() { if (true == false) return 1; return 0; }
function constJeqTrueTrue() { if (true == true) return 1; return 0; }
function constJneqFalseTrue() { if (false != true) return 1; return 0; }
function constJneqFalseFalse() { if (false != false) return 1; return 0; }
var constFunctions = [constEqTrueFalse, constEqTrueTrue, constNeqFalseTrue, constNeqFalseFalse, constJeqTrueFalse, constJeqTrueTrue, constJneqFalseTrue, constJneqFalseFalse];
for (var f of constFunctions) {
    noInline(f);
    noDFG(f);
}
for (var i = 0; i < testLoopCount; ++i) {
    if (constEqTrueFalse() !== false || constEqTrueTrue() !== true || constNeqFalseTrue() !== true || constNeqFalseFalse() !== false)
        throw new Error("constant boolean value equality");
    if (constJeqTrueFalse() !== 0 || constJeqTrueTrue() !== 1 || constJneqFalseTrue() !== 1 || constJneqFalseFalse() !== 0)
        throw new Error("constant boolean jump equality");
}
