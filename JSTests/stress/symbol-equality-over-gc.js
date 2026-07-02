function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    let symbol = Symbol();
    let object1 = {
        [symbol]: 42
    }
    let object2 = {
        [symbol]: 42
    }
    symbol = null;
    fullGC();
    shouldBe(Object.getOwnPropertySymbols(object1)[0], Object.getOwnPropertySymbols(object2)[0]);
}
noInline(test);

for (let i = 0; i < 1000; ++i)
    test();
