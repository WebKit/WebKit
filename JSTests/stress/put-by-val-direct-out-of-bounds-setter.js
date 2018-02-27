var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var testInt32 = createBuiltin(`(function (array, index, value) {
    @putByValDirect(array, index, value);
})`);
noInline(testInt32);

Object.defineProperty(Array.prototype, 42, {
    get() {
        return 30;
    },
    set(value) {
    }
});
for (var i = 0; i < 1e5; ++i) {
    var array = [1, 2, 3, 4, 5];
    shouldBe(array[42], 30);
    testInt32(array, 42, 42);
    shouldBe(array[42], 42);
    shouldBe(Array.prototype[42], 30);
}
