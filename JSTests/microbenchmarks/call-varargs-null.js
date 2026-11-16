function test2()
{
}

function test(input)
{
    return test2.apply(null, input);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(null);
