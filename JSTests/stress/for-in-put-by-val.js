function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    var object = {
        a: 42,
        b: 43,
        c: 44,
        d: 45,
        e: 46,
        f: 47,
        g: 48,
        h: 49,
    };

    for (var i in object) {
        var value = object[i] + 20;
        object[i] = value;
    }
    return object;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i) {
    var object = test();
    if (i & 0x100) {
        var k = 62;
        for (var i in object)
            shouldBe(object[i], k++);
    }
}
