function test()
{
    for (var i = 0; i < 1e6; ++i)
        (i * 0.1).toString();
}
noInline(test);

test();
