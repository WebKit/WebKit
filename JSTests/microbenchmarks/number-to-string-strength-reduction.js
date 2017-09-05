function test()
{
    var target = 42;
    return target.toString(16);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
