function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, val1)
{
    return array.push(val1);
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var array = ["Cocoa"];
    ensureArrayStorage(array);
    array.length = 2;
    shouldBe(test(array, "Cocoa"), 3);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], undefined);
    shouldBe(array[2], "Cocoa");
}

var array = ["Cocoa"];
ensureArrayStorage(array);
array.length = 0x7fffffff;
shouldBe(test(array, "Cocoa"), 0x7fffffff + 1);
shouldBe(array[0x7fffffff], "Cocoa");
