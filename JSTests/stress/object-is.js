function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function is1(a, b) { return Object.is(a, b); }
noInline(is1);
function is2(a, b) { return Object.is(a, b); }
noInline(is2);
function is3(a, b) { return Object.is(a, b); }
noInline(is3);
function is4(a, b) { return Object.is(a, b); }
noInline(is4);
function is5(a, b) { return Object.is(a, b); }
noInline(is5);
function is6(a, b) { return Object.is(a, b); }
noInline(is6);
function is7(a, b) { return Object.is(a, b); }
noInline(is7);
function is8(a, b) { return Object.is(a, b); }
noInline(is8);
function is9(a, b) { return Object.is(a, b); }
noInline(is9);
function is10(a, b) { return Object.is(a, b); }
noInline(is10);
function is11(a, b) { return Object.is(a, b); }
noInline(is11);
function is12(a, b) { return Object.is(a, b); }
noInline(is12);
function is13(a, b) { return Object.is(a, b); }
noInline(is13);
function is14(a, b) { return Object.is(a, b); }
noInline(is14);
function is15(a, b) { return Object.is(a, b); }
noInline(is15);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(Object.is(NaN, NaN), true);
    shouldBe(Object.is(null, null), true);
    shouldBe(Object.is(null), false);
    shouldBe(Object.is(undefined, undefined), true);
    shouldBe(Object.is(true, true), true);
    shouldBe(Object.is(false, false), true);
    shouldBe(Object.is('abc', 'abc'), true);
    shouldBe(Object.is(Infinity, Infinity), true);
    shouldBe(Object.is(0, 0), true);
    shouldBe(Object.is(-0, -0), true);
    shouldBe(Object.is(0, -0), false);
    shouldBe(Object.is(-0, 0), false);
    var obj = {};
    shouldBe(Object.is(obj, obj), true);
    var arr = [];
    shouldBe(Object.is(arr, arr), true);
    var sym = Symbol();
    shouldBe(Object.is(sym, sym), true);

    shouldBe(is1(NaN, NaN), true);
    shouldBe(is2(null, null), true);
    shouldBe(is3(null), false);
    shouldBe(is4(undefined, undefined), true);
    shouldBe(is5(true, true), true);
    shouldBe(is6(false, false), true);
    shouldBe(is7('abc', 'abc'), true);
    shouldBe(is8(Infinity, Infinity), true);
    shouldBe(is9(0, 0), true);
    shouldBe(is10(-0, -0), true);
    shouldBe(is11(0, -0), false);
    shouldBe(is12(-0, 0), false);
    shouldBe(is13(obj, obj), true);
    shouldBe(is14(arr, arr), true);
    shouldBe(is15(sym, sym), true);
}
