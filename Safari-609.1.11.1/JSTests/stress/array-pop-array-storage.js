function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array) {
    return [array.pop(), array.pop(), array.pop(), array.pop()];
}

noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var array = ["foo", "bar", "baz"];
    ensureArrayStorage(array);
    var result = test(array);
    shouldBe(result[0], "baz");
    shouldBe(result[1], "bar");
    shouldBe(result[2], "foo");
    shouldBe(result[3], undefined);
    shouldBe(array.length, 0);
}

for (var i = 0; i < 1e4; ++i) {
    var array = ["foo", "bar", , "baz"];
    ensureArrayStorage(array);
    var result = test(array);
    shouldBe(result[0], "baz");
    shouldBe(result[1], undefined);
    shouldBe(result[2], "bar");
    shouldBe(result[3], "foo");
    shouldBe(array.length, 0);
}

for (var i = 0; i < 1e4; ++i) {
    var array = ["foo", "bar", , "baz", , , "OK"];
    ensureArrayStorage(array);
    shouldBe(array.length, 7);
    var result = test(array);
    shouldBe(result[0], "OK");
    shouldBe(result[1], undefined);
    shouldBe(result[2], undefined);
    shouldBe(result[3], "baz");
    shouldBe(array.length, 3);
    shouldBe(array[0], "foo");
    shouldBe(array[1], "bar");
    shouldBe(array[2], undefined);
    shouldBe(array[3], undefined);
}

for (var i = 0; i < 1e4; ++i) {
    var array = ["foo", "bar", "baz"];
    ensureArrayStorage(array);
    array.length = 0xffffffff - 1;
    shouldBe(array.length, 0xffffffff - 1);
    var result = test(array);
    shouldBe(result[0], undefined);
    shouldBe(result[1], undefined);
    shouldBe(result[2], undefined);
    shouldBe(result[3], undefined);
    shouldBe(array.length, 0xffffffff - 5);
    shouldBe(array[0], "foo");
    shouldBe(array[1], "bar");
    shouldBe(array[2], "baz");
    shouldBe(array[3], undefined);
}
