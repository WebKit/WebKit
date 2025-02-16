function test()
{
    for (var i = 0; i < testLoopCount; ++i)
        fiatInt52(i).toString();
}
noInline(test);

test();
