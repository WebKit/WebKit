function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var count = 0;
Object.defineProperty(Array.prototype, 0, {
    set() {
        ++count;
    }
});

function test() {
    var array = [];
    for (var i = 0; i < 10; ++i)
        array.push(i, i + 1, i + 2);
    return array;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldBe(test().length, 30);
shouldBe(count, 10000);
