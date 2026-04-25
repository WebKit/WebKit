function test()
{
    for (var i = 0; i < testLoopCount; ++i)
        (i * 0.1).toString();
}
noInline(test);

test();
