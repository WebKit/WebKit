//@ slow!

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(num)
{
    var regexp = /hello world/;
    regexp.lastIndex = { ok: regexp, value: 42 };
    if (num === 0)
        return regexp;
    if (num === 1)
        return regexp.lastIndex;
    return regexp.lastIndex.value;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var num = i % 3;
    switch (num) {
    case 0:
        var regexp = test(num);
        shouldBe(regexp instanceof RegExp, true);
        shouldBe(typeof regexp.lastIndex, "object");
        shouldBe(regexp.lastIndex.ok, regexp);
        break;
    case 1:
        var object = test(num);
        shouldBe(object.value, 42);
        shouldBe(object.ok instanceof RegExp, true);
        shouldBe(object.ok.lastIndex, object);
        break;
    case 2:
        var value = test(num);
        shouldBe(value, 42);
        break;
    }
}
