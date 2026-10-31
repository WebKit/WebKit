function test()
{
    return [1, 2, 3, 4];
}
noInline(test);
noDFG(test);

for (var i = 0; i < 1e6; ++i)
    test();
