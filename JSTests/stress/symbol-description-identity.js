function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(description)
{
    return Symbol(description);
}
noInline(test);

var set = new Set;
for (var i = 0; i < 1e4; ++i) {
    var description = String(i);
    var symbol = test(description);
    set.add(symbol);
    shouldBe(set.size, i + 1);
    shouldBe(symbol.description, description);
}
