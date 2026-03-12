function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(base) {
    return base.ok;
}
noInline(test);

Object.prototype.ok = 42;

var array = [
];
for (var i = 0; i < 16; ++i) {
    array.push({
        ["Hello" + i]: i
    });
}
array.push("Hey");

for (var i = 0; i < testLoopCount; ++i) {
    for (var v of array)
        shouldBe(test(v), 42);
}

shouldBe(test("Hey"), 42);
String.prototype.ok = 0;
shouldBe(test("Hey"), 0);
shouldBe(test("OK"), 0);
