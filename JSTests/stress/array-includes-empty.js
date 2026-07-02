//@ runDefault("--useConcurrentJIT=0")

function test()
{
    const v5 = [-9007199254740990];
    v5[4] = v5;
    return v5["includes"]("includes");
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test();
