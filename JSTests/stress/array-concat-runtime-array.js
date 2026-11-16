function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = $vm.createRuntimeArray();
var concat = [].concat(array);
shouldBe(concat.length, 0);
