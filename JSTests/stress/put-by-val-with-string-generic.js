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

var string = 'hello';
for (var i = 0; i < 10001; ++i) {
    var object = {};
    if (i === 10000) {
        assign(object, 'testing', 42);
        shouldBe(object.testing, 42);
        shouldBe(object.hello, undefined);
    } else {
        assign(object, string, 42);
        shouldBe(object[string], 42);
    }
}

