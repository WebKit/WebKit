function test()
{
    for (var i = 0; i < 1e6; ++i)
        i.toString();
}
noInline(test);

test();
