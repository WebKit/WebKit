var symbol1 = Symbol();
var symbol2 = Object.getOwnPropertySymbols({ [symbol1]: 42 })[0];

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function getByVal(object, name)
{
    return object[name];
}
noInline(getByVal);

function getSym1()
{
    return symbol1;
}
noInline(getSym1);

function getSym2()
{
    return symbol2;
}
noInline(getSym2);

var object = {
    [symbol1]: 42,
    hello: 50
};

for (var i = 0; i < 10000; ++i)
    shouldBe(getByVal(object, i % 2 === 0 ? getSym1() : getSym2()), 42);
