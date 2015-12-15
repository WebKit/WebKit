function test()
{
    var result = false;
    for (var i = 0; i < 100; ++i) {
        if (Math.random() < 0.5)
            result = true;
        if (Math.random() >= 0.5)
            result = true;
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
