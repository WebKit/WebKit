function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var set = new Set();
for (var i = 0; i < 100; ++i)
    set.add(i);

function test(set) {
    var result = 0;
    set.forEach((a) => {
        result += a;
    });
    return result;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(set), 4950);
