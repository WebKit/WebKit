function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const sameValue = $vm.createBuiltin(`(function (a, b) {
    return @sameValue(a, b);
})`);

const obj = {};
const arr = [];
const sym = Symbol();

for (let i = 0; i < 1e5; ++i) {
    shouldBe(sameValue(null, null), true);
    shouldBe(sameValue(null, undefined), false);
    shouldBe(sameValue(true, true), true);
    shouldBe(sameValue(true, false), false);
    shouldBe(sameValue('abc', 'abc'), true);
    shouldBe(sameValue(NaN, NaN), true);
    shouldBe(sameValue(Infinity, Infinity), true);
    shouldBe(sameValue(0, 0), true);
    shouldBe(sameValue(-0, -0), true);
    shouldBe(sameValue(0, -0), false);
    shouldBe(sameValue(-0, 0), false);
    shouldBe(sameValue(obj, obj), true);
    shouldBe(sameValue({}, {}), false);
    shouldBe(sameValue(arr, arr), true);
    shouldBe(sameValue([], []), false);
    shouldBe(sameValue(sym, sym), true);
}
