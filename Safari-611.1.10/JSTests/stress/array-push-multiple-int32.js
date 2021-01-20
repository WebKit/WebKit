//@ skip if $architecture == "x86"

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, val1, val2, val3)
{
    return array.push(val1, val2, val3);
}
noInline(test);

for (var i = 0; i < 1e7; ++i) {
    var array = [];
    shouldBe(test(array, 1, 2, 3), 3);
    shouldBe(array[0], 1);
    shouldBe(array[1], 2);
    shouldBe(array[2], 3);
}
var array = [];
shouldBe(test(array, 1, 2, 3.3), 3);
shouldBe(array[0], 1);
shouldBe(array[1], 2);
shouldBe(array[2], 3.3);
