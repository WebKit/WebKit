function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function assign(object, name, value)
{
    object[name] = value;
}
noInline(assign);

var key = Symbol();
for (var i = 0; i < 10001; ++i) {
    var object = {};
    if (i === 10000) {
        var key2 = 42;
        assign(object, key2, 42);
        shouldBe(object[key2], 42);
        shouldBe(object[key], undefined);
    } else {
        assign(object, key, 42);
        shouldBe(object[key], 42);
    }
}
