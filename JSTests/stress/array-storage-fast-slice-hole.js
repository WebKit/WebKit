function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array1 = [0, 1, 2, 3,,, 4, 5];
$vm.ensureArrayStorage(array1);

function test(array) {
    var sliced = array.slice();
    shouldBe(JSON.stringify(sliced), `[0,1,2,3,null,null,4,5]`);
    shouldBe(4 in array, false);
    shouldBe(4 in sliced, false);
}

for (var i = 0; i < 1e4; ++i)
    test(array1);
