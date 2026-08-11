function test()
{
    return new Map();
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test();
