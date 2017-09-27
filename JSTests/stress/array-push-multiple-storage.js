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

for (var i = 0; i < 1e5; ++i) {
    var array = [];
    ensureArrayStorage(array);
    shouldBe(test(array, "Cocoa", "Cappuccino", "Matcha"), 3);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], "Cappuccino");
    shouldBe(array[2], "Matcha");
}
for (var i = 0; i < 1e5; ++i) {
    var array = [0];
    ensureArrayStorage(array);
    shouldBe(test(array, "Cocoa", "Cappuccino", "Matcha"), 4);
    shouldBe(array[0], 0);
    shouldBe(array[1], "Cocoa");
    shouldBe(array[2], "Cappuccino");
    shouldBe(array[3], "Matcha");
}
