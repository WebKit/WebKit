function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array  = [];
for (var i = 0; i < 0x100; ++i) {
    var object = {
        test: 42,
        ["prop" + i]: 42
    };
    $vm.toCacheableDictionary(object);
    array.push(object);
}
function access(object) { return object.test2; }

for (var i = 0; i < 1e6; ++i) {
    var index = i & (0x100 - 1);
    var object = array[index];
    shouldBe(access(object), undefined);
}
array[0].test2 = 42;
shouldBe(access(array[0]), 42);
