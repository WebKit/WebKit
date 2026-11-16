function test(index)
{
    var source = () => index;
    source.first = 1;
    source.second = 2;
    source.third = 3;
    source.fourth = 4;
    var { first, ...rest } = source;
    return rest;
}
noInline(test);

for (var i = 0; i < 5e5; ++i)
    test(i);
