function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function forIn(object)
{
    var iteration = 0;
    for (var i in object)
        ++iteration;
    return iteration;
}
noInline(forIn);

var object = { i: 42, i2: 43, i3: 44 };
var object2 = { i: 42, i2: 43, i3: 44, i4:45 };
for (var i = 0; i < 1e5; ++i) {
    shouldBe(forIn(null), 0);
    shouldBe(forIn(object), 3);
    shouldBe(forIn(undefined), 0);
    shouldBe(forIn(object2), 4);
}
