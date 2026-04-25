function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [42, "Hello", 32, "World", -333];
shouldBe(array.join(""), `42Hello32World-333`);
shouldBe(array.join(","), `42,Hello,32,World,-333`);
array.pop();
array.pop();
array.pop();
array.pop();
shouldBe(array.join(""), `42`);
shouldBe(array.join(","), `42`);
