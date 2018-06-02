function test()
{
    return Object.create(null);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
