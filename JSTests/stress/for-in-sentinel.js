function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    $: 32,
    test: 33,
    hey: 34,
};

function test(object) {
    var count = 0;
    for (var i in object) {
        ++count;
    }
    return count;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(object), 3);
