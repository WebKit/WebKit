function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(src, dest) {
    for (var key in src)
        dest[key] = src[key] + 42;
}

var source = {
    a: 0, b: 1, c: 2, d: 3, e: 4
};

for (var i = 0; i < testLoopCount; ++i) {
    var dest = { };
    test(source, dest);
    shouldBe(JSON.stringify(dest), `{"a":42,"b":43,"c":44,"d":45,"e":46}`);
}
