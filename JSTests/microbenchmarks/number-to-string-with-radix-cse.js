function test()
{
    for (var i = 0; i < 1e2; ++i) {
        i.toString(16);
        i.toString(16);
        i.toString(16);
        i.toString(16);
        i.toString(16);
    }
}
noInline(test);

for (var i = 0; i < 1e3; ++i)
    test();
