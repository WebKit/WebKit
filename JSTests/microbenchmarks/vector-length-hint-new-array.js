function test(v0, v1)
{
    var array = [v0, v1];
    for (var i = 0; i < 20; ++i)
        array.push(i);
    return array;
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(1, 2);
