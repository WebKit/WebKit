function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test()
{
    return Symbol();
}
noInline(test);

var set = new Set;
for (var i = 0; i < 1e4; ++i) {
    var symbol = test();
    set.add(symbol);
    shouldBe(set.size, i + 1);
    shouldBe(symbol.description, undefined);
}
