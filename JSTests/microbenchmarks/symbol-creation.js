function test()
{
    return Symbol();
}
noInline(test);

for (var i = 0; i < 4e5; ++i)
    test();
