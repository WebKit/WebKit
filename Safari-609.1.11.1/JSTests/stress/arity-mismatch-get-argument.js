var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var builtin = createBuiltin(`(function () {
    return @argument(0);
})`);

function test()
{
    var result = builtin();
    shouldBe(result, undefined);
    var result = builtin(42);
    shouldBe(result, 42);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
