function test()
{
    for (var i = 0; i < 1e6; ++i)
        fiatInt52(i).toString();
}
noInline(test);

test();
