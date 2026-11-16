function test()
{
    return new WeakSet();
}
noInline(test);

for (var i = 0; i < 2e6; ++i)
    test();
