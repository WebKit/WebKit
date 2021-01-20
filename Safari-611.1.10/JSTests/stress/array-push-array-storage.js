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
    shouldBe(test(array, "Cocoa"), 2);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], "Cocoa");
    shouldBe(array[2], undefined);
    shouldBe(array[3], undefined);
    shouldBe(array[4], undefined);
    shouldBe(test(array, "Cappuccino"), 3);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], "Cocoa");
    shouldBe(array[2], "Cappuccino");
    shouldBe(array[3], undefined);
    shouldBe(array[4], undefined);
    shouldBe(test(array, "Matcha"), 4);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], "Cocoa");
    shouldBe(array[2], "Cappuccino");
    shouldBe(array[3], "Matcha");
    shouldBe(array[4], undefined);
    shouldBe(test(array, "Matcha"), 5);
    shouldBe(array[0], "Cocoa");
    shouldBe(array[1], "Cocoa");
    shouldBe(array[2], "Cappuccino");
    shouldBe(array[3], "Matcha");
    shouldBe(array[4], "Matcha");
}
