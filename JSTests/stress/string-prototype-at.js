function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function stringAt(string, index) {
    return string.at(index);
}
noInline(stringAt);

function test(str, index, expected) {
    let count = 0;
    for (let i = 0; i < testLoopCount; i++) {
        if (stringAt(str, index) === expected)
          count++;
    }
    return count;
}
noInline(test);

const string8 = "hello, world";
const string16 = "こんにちは、世界";

shouldBe(test(string16, 0, "こ"), testLoopCount);
shouldBe(test(string16, 2, "に"), testLoopCount);
shouldBe(test(string16, 20, undefined), testLoopCount);
shouldBe(test(string16, -2, "世"), testLoopCount);
shouldBe(test(string16, -40, undefined), testLoopCount);

shouldBe(test(string8, 0, "h"), testLoopCount);
shouldBe(test(string8, 2, "l"), testLoopCount);
shouldBe(test(string8, 20, undefined), testLoopCount);
shouldBe(test(string8, -2, "l"), testLoopCount);
shouldBe(test(string8, -40, undefined), testLoopCount);
