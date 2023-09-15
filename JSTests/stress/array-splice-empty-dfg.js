function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array) {
    return array.splice(0, 0);
}
noInline(test);

var array = [];
for (var i = 0; i < 1e6; ++i) {
    shouldBe(Array.isArray(test(array)), true);
}
