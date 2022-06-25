function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function gen1(value)
{
    return 'he' + value;
}
noInline(gen1);

function gen2(value)
{
    return value + 'ld';
}
noInline(gen2);

function assign(object, name, value)
{
    object[name] = value;
}
noInline(assign);

for (var i = 0; i < 10000; ++i) {
    var object = {};
    if (i % 2 === 0) {
        assign(object, gen1('llo'), 42);
        shouldBe(object.hello, 42);
    } else {
        assign(object, gen2('wor'), 42);
        shouldBe(object.world, 42);
    }
}
