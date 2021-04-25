function test(array)
{
    return new Map(array);
}
noInline(test);

var array = [];

for (var j = 0; j < 50; ++j)
    array[j] = [j, 0];

for (var i = 0; i < 1e4; ++i)
    test(array);
