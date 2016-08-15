function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function getByVal(object, name)
{
    return object[name];
}
noInline(getByVal);

function getStr1()
{
    return "hello";
}
noInline(getStr1);

function getStr2()
{
    return "hello";
}
noInline(getStr2);

var object = {
    hello: 42,
    world: 50
};

for (var i = 0; i < 10000; ++i)
    shouldBe(getByVal(object, i % 2 === 0 ? getStr1() : getStr2()), 42);
shouldBe(getByVal(object, 'world'), 50);
