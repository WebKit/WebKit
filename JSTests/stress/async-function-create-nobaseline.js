// This test requires ENABLE_ES2017_ASYNCFUNCTION_SYNTAX to be enabled at build time.
//@ skip

function test(cond)
{
    if (cond) {
        var func = async function () {};
        return func;
    }

    return 42;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
