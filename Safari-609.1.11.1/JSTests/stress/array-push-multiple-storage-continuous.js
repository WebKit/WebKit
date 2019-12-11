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

var array = [];
ensureArrayStorage(array);
for (var i = 0; i < 1e3; ++i) {
    shouldBe(test(array, "Cocoa", "Cappuccino", "Matcha"), 3 * (i + 1));
}
for (var i = 0; i < 1e3; ++i) {
    shouldBe(array[3 * i + 0], "Cocoa");
    shouldBe(array[3 * i + 1], "Cappuccino");
    shouldBe(array[3 * i + 2], "Matcha");
}
