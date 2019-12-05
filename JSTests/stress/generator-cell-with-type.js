// This test takes too long on arm and mips devices.
//@ skip if ["arm", "mips"].include?($architecture)
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function *generator()
{
}

function test(gen)
{
    var func = gen.next;
    shouldBe(gen.next().done, true);
    return func;
}
noInline(test);
var gen = generator();
for (var i = 0; i < 1e6; ++i)
    test(gen);

for (var i = 0; i < 1e6; ++i) {
    test(gen);
    shouldThrow(() => {
        test({
            __proto__: gen.__proto__
        });
    }, `TypeError: |this| should be a generator`);
}
