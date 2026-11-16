function test()
{
    return new Set();
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test();
