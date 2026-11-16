//@ runDefault
//@ runFTLNoCJIT

function eq(a, b) {
    return a === b;
}
noInline(eq);

var poison1 = [1.5, null, {}, "x", true, 0, undefined];
var poison2 = [2.5, undefined, [], "y", false, 1, null];
for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < poison1.length; ++j)
        eq(poison1[j], poison2[j]);
}

var failures = [];
function check(a, b, expected, label) {
    var got = eq(a, b);
    if (got !== expected)
        failures.push(label + ": eq(" + String(a) + ", " + String(b) + ") = " + got + ", expected " + expected);
}

check(0.0, 0.0, true, "double-zero-vs-double-zero");
check(0.0, null, false, "double-zero-vs-null");
check(null, 0.0, false, "null-vs-double-zero");
check(0.0, 0, true, "double-zero-vs-int-zero");
check(0, 0.0, true, "int-zero-vs-double-zero");

check(-0.0, -0.0, true, "neg-zero-vs-neg-zero");
check(-0.0, 0.0, true, "neg-zero-vs-pos-zero");
check(-0.0, 0, true, "neg-zero-vs-int-zero");

check(5e-324, 5e-324, true, "denormal-vs-denormal");
check(5e-324, 0.0, false, "denormal-vs-zero");
check(5e-324, 0, false, "denormal-vs-int-zero");
check(-5e-324, -5e-324, true, "neg-denormal-vs-neg-denormal");
check(-5e-324, 5e-324, false, "neg-denormal-vs-pos-denormal");

check(NaN, NaN, false, "nan-vs-nan");
check(NaN, 0.0, false, "nan-vs-zero");
check(NaN, null, false, "nan-vs-null");
check(NaN, undefined, false, "nan-vs-undefined");
check(NaN, 0, false, "nan-vs-int-zero");
check(NaN, {}, false, "nan-vs-object");

check(Infinity, Infinity, true, "inf-vs-inf");
check(-Infinity, -Infinity, true, "neg-inf-vs-neg-inf");
check(Infinity, -Infinity, false, "inf-vs-neg-inf");
check(Infinity, Number.MAX_VALUE, false, "inf-vs-max-value");

check(Number.MAX_VALUE, Number.MAX_VALUE, true, "max-value-vs-max-value");
check(Number.MAX_VALUE, Number.MAX_VALUE - 1e300, false, "max-value-vs-smaller");

check(2147483648.0, 2147483648.0, true, "2^31-double-vs-self");
check(-2147483649.0, -2147483649.0, true, "below-int32-min-double-vs-self");
check(4294967296.0, 4294967296.0, true, "2^32-double-vs-self");

var others = [null, undefined, true, false];
var otherExpected = [
    [true, false, false, false],
    [false, true, false, false],
    [false, false, true, false],
    [false, false, false, true],
];
for (var i = 0; i < 4; ++i)
    for (var j = 0; j < 4; ++j)
        check(others[i], others[j], otherExpected[i][j], "other-" + i + "-vs-" + j);

check(0, 0, true, "int-zero-vs-int-zero");
check(-1, -1, true, "int-neg1-vs-int-neg1");
check(2147483647, 2147483647, true, "int32-max-vs-int32-max");
check(-2147483648, -2147483648, true, "int32-min-vs-int32-min");
check(2147483647, -2147483648, false, "int32-max-vs-int32-min");
check(0, 1, false, "int-zero-vs-int-one");
check(0, -1, false, "int-zero-vs-int-neg1");

check(0, null, false, "int-zero-vs-null");
check(0, undefined, false, "int-zero-vs-undefined");
check(0, false, false, "int-zero-vs-false");
check(1, true, false, "int-one-vs-true");
check(2, null, false, "int-two-vs-null");
check(6, false, false, "int-six-vs-false");
check(7, true, false, "int-seven-vs-true");
check(10, undefined, false, "int-ten-vs-undefined");

check(5, 5.0, true, "int5-vs-double5");
check(0, 0.0, true, "int0-vs-double0");
check(-1, -1.0, true, "int-neg1-vs-double-neg1");
check(2147483647, 2147483647.0, true, "int32-max-vs-double-same");
check(-2147483648, -2147483648.0, true, "int32-min-vs-double-same");

check(5, 5.5, false, "int5-vs-double5.5");
check(5, 5.0000000001, false, "int5-vs-double-almost5");

var objA = {};
var objB = {};
check(objA, objA, true, "obj-vs-self");
check(objA, objB, false, "obj-vs-other-obj");
check(objA, null, false, "obj-vs-null");
check(objA, undefined, false, "obj-vs-undefined");
check(objA, true, false, "obj-vs-true");
check(objA, false, false, "obj-vs-false");
check(objA, 0, false, "obj-vs-int-zero");
check(objA, 42, false, "obj-vs-int-42");
check(null, objA, false, "null-vs-obj");
check(42, objA, false, "int-42-vs-obj");

var strA = "hello" + Math.random().toString();
var strB = "hello" + Math.random().toString();
var strC = "world";
var strD = "wor" + "ld";
check(strA, strA, true, "string-vs-self");
check(strA, strB, false, "string-vs-diff-string");
check(strC, strD, true, "atom-vs-folded-atom");
check(strA, null, false, "string-vs-null");
check(strA, 0, false, "string-vs-int-zero");
check("", "", true, "empty-string-vs-empty-string");
check("", null, false, "empty-string-vs-null");

function makeString(s) { return s + ""; }
noInline(makeString);
var same1 = makeString("same content here");
var same2 = makeString("same content here");
check(same1, same2, true, "same-content-diff-cells-maybe");

var symA = Symbol("a");
var symB = Symbol("a");
check(symA, symA, true, "symbol-vs-self");
check(symA, symB, false, "symbol-vs-diff-symbol");
check(symA, null, false, "symbol-vs-null");
check(symA, "a", false, "symbol-vs-string");
check(Symbol.iterator, Symbol.iterator, true, "wellknown-symbol-vs-self");

var fnA = function() {};
var fnB = function() {};
check(fnA, fnA, true, "fn-vs-self");
check(fnA, fnB, false, "fn-vs-diff-fn");
check(fnA, objA, false, "fn-vs-obj");

var arrA = [];
var arrB = [];
check(arrA, arrA, true, "arr-vs-self");
check(arrA, arrB, false, "arr-vs-diff-arr");

check(1n, 1n, true, "bigint-1-vs-bigint-1");
check(1n, 2n, false, "bigint-1-vs-bigint-2");
check(0n, 0n, true, "bigint-0-vs-bigint-0");
check(-1n, -1n, true, "bigint-neg1-vs-bigint-neg1");
check(12345678901234567890n, 12345678901234567890n, true, "bigint-large-vs-same");
check(12345678901234567890n, 12345678901234567891n, false, "bigint-large-vs-diff");
check(1n, 1, false, "bigint-vs-int");
check(1n, 1.0, false, "bigint-vs-double");
check(1n, "1", false, "bigint-vs-string");
check(1n, true, false, "bigint-vs-true");
check(0n, false, false, "bigint-0-vs-false");
check(0n, null, false, "bigint-0-vs-null");
check(0n, 0, false, "bigint-0-vs-int-0");
check(0n, 0.0, false, "bigint-0-vs-double-0");

check(1.5, null, false, "double-vs-null");
check(1.5, undefined, false, "double-vs-undefined");
check(1.5, true, false, "double-vs-true");
check(1.5, false, false, "double-vs-false");
check(1.5, objA, false, "double-vs-obj");
check(1.5, strA, false, "double-vs-string");
check(1.5, symA, false, "double-vs-symbol");
check(1.5, 1n, false, "double-vs-bigint");

for (var iter = 0; iter < testLoopCount; ++iter) {
    check(null, null, true, "loop-null-null");
    check(null, undefined, false, "loop-null-undefined");
    check(true, false, false, "loop-true-false");
    check(5, 5, true, "loop-5-5");
    check(objA, objA, true, "loop-obj-self");
    check(objA, objB, false, "loop-obj-diff");
    check(objA, null, false, "loop-obj-null");
    check(1.5, 1.5, true, "loop-double-double");
    check(NaN, NaN, false, "loop-nan-nan");
    check(0, -0.0, true, "loop-int0-neg0");
    check(1n, 1n, true, "loop-bigint");
    check(strA, strA, true, "loop-string-self");
    check(6, false, false, "loop-int6-false");
    check(symA, symB, false, "loop-symbol-diff");
}

function neq(a, b) {
    return a !== b;
}
noInline(neq);

for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < poison1.length; ++j)
        neq(poison1[j], poison2[j]);
}

function checkNeq(a, b, expected, label) {
    var got = neq(a, b);
    if (got !== expected)
        failures.push("neq " + label + ": " + got + " expected " + expected);
}

for (var iter = 0; iter < testLoopCount; ++iter) {
    checkNeq(null, null, false, "null-null");
    checkNeq(null, undefined, true, "null-undefined");
    checkNeq(true, true, false, "true-true");
    checkNeq(true, false, true, "true-false");
    checkNeq(5, 5, false, "5-5");
    checkNeq(5, 7, true, "5-7");
    checkNeq(objA, objA, false, "obj-self");
    checkNeq(objA, objB, true, "obj-diff");
    checkNeq(objA, null, true, "obj-null");
    checkNeq(1.5, 1.5, false, "1.5-1.5");
    checkNeq(NaN, NaN, true, "nan-nan");
    checkNeq(0, -0.0, false, "int0-neg0");
    checkNeq(0.0, 0, false, "double0-int0");
    checkNeq(1n, 1n, false, "bigint-1-1");
    checkNeq(1n, 2n, true, "bigint-1-2");
    checkNeq(strA, strA, false, "string-self");
    checkNeq(6, false, true, "int6-false");
}

if (failures.length)
    throw new Error("FAILURES:\n" + failures.join("\n"));
