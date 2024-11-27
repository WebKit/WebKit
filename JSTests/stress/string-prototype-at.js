function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function stringAt(string, index) {
    return string.at(index);
}
noInline(stringAt);

const runs = 1e6;
function test(str, index, expected) {
    let count = 0;
    for (let i = 0; i < runs; i++) {
        if (stringAt(str, index) === expected)
          count++;
    }
    return count;
}
noInline(test);

const string8 = "hello, world";
const string16 = "こんにちは、世界";

shouldBe(test(string16, 0, "こ"), runs);
shouldBe(test(string16, 2, "に"), runs);
shouldBe(test(string16, 20, undefined), runs);
shouldBe(test(string16, -2, "世"), runs);
shouldBe(test(string16, -40, undefined), runs);

shouldBe(test(string8, 0, "h"), runs);
shouldBe(test(string8, 2, "l"), runs);
shouldBe(test(string8, 20, undefined), runs);
shouldBe(test(string8, -2, "l"), runs);
shouldBe(test(string8, -40, undefined), runs);
