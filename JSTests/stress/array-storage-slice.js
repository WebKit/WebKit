function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array1 = [0, 1, 2, 3, 4, 5];
$vm.ensureArrayStorage(array1);

var array2 = [0, 1, 2, 3, 4, 5];
array2.length = 42;
$vm.ensureArrayStorage(array2);

function test(array) {
    return array.slice();
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(JSON.stringify(test(array1)), `[0,1,2,3,4,5]`);
    shouldBe(JSON.stringify(test(array2)), `[0,1,2,3,4,5,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null]`);
}
