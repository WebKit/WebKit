function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var set = new Set;
for (var i = 0; i < 100; ++i)
    set.add(i);

function test(set) {
    var result = 0;
    for (let a of set) {
        result += a;
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(set), 4950);
