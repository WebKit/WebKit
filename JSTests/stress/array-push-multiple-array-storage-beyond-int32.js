function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, val1, val2)
{
    return array.push(val1, val2);
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var array = ["Cocoa"];
    ensureArrayStorage(array);
    array.length = 2;
    shouldBe(test(array, "Cocoa", "Cappuccino"), 4);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], undefined);
    shouldBe(array[2], "Cocoa");
    shouldBe(array[3], "Cappuccino");
}

var array = ["Cocoa"];
ensureArrayStorage(array);
array.length = 0x7fffffff - 1;
shouldBe(test(array, "Cocoa", "Cappuccino"), 0x7fffffff + 1);
shouldBe(array[0x7fffffff - 1], "Cocoa");
shouldBe(array[0x7fffffff - 0], "Cappuccino");
