function test()
{
    return new WeakMap();
}
noInline(test);

for (var i = 0; i < 2e6; ++i)
    test();
