function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(target)
{
    var regexp = /OK/g;
    regexp[Symbol.replace] = function() { return "Hello"; };
    return "OKOK".replace(regexp, target);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test("Replaced"), `Hello`);
