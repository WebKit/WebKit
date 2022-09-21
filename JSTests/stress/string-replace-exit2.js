function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(target)
{
    return "OKOK".replace(/OK/g, target);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test("Replaced"), `ReplacedReplaced`);
RegExp.prototype[Symbol.replace] = function() { return "Hello"; }
shouldBe(test("Replaced"), `Hello`);
