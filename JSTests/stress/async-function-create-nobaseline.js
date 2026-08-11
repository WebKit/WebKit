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
