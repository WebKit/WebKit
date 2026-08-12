function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, separator) {
    return array.join(separator);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test([], ","), ``);
    shouldBe(test([42], ","), `42`);
    shouldBe(test([-42], ", "), `-42`);
    shouldBe(test([1, 2, 3], ","), `1,2,3`);
    shouldBe(test([1, 2, 3], ", "), `1, 2, 3`);
    shouldBe(test([1, 2, 3], ""), `123`);
    shouldBe(test([1, 2, 3], undefined), `1,2,3`);
    shouldBe(test([1, 2, 3], "、"), `1、2、3`);
    shouldBe(test([0, -1, 2147483647, -2147483648], "&id="), `0&id=-1&id=2147483647&id=-2147483648`);
    shouldBe([10, 200, 3000].toString(), `10,200,3000`);

    var array = new Array(3);
    array[0] = 1;
    array[2] = 3;
    shouldBe(test(array, "-"), `1--3`);
    shouldBe(test([1, , 3], ", "), `1, , 3`);
}

Array.prototype[1] = "x";
shouldBe(test([1, , 3], "-"), `1-x-3`);
var array = new Array(3);
array[0] = 1;
array[2] = 3;
shouldBe(test(array, ", "), `1, x, 3`);
