function test(array)
{
    return new WeakSet(array);
}
noInline(test);

var array = [];

for (var j = 0; j < 50; ++j)
    array[j] = {};

for (var i = 0; i < 1e4; ++i)
    test(array);
