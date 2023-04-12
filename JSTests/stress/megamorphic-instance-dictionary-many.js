function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array  = [];
var proto = {
    test2: 0,
};

for (var i = 0; i < 0x100; ++i) {
    var object = {
        __proto__: proto,
        test: 42,
    };
    for (var j = 0; j < 1000; ++j)
        object['prop' + j] = j;
    object['id' + i] = 42;
    $vm.toCacheableDictionary(object);
    array.push(object);
}
function access(object) { return object.prop999; }

for (var i = 0; i < 1e6; ++i) {
    var index = i & (0x100 - 1);
    var object = array[index];
    shouldBe(access(object), 999);
}
array[0].prop999 = 42;
shouldBe(access(array[0]), 42);
array[0].test999 = 42;
shouldBe(access(array[0]), 42);
