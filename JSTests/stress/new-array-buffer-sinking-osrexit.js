function target(p, a0, a1, a2, a3, a4, a5)
{
    if (p)
        OSRExit();
    return a5
}

function test(p)
{
    var array = [1,2,3,4,5];
    return target(p, ...array);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(false);
test(true);
