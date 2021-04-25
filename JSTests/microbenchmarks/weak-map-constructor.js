function test(array)
{
    return new WeakMap(array);
}
noInline(test);

var array = [];

for (var j = 0; j < 50; ++j)
    array[j] = [{}, 0];

for (var i = 0; i < 1e4; ++i)
    test(array);
