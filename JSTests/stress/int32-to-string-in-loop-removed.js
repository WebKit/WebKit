function test()
{
    for (var i = 0; i < testLoopCount; ++i)
        i.toString();
}
noInline(test);

test();
