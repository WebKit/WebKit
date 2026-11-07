function test(value)
{
    var array = [1, 2, 3, 4];
    array[0] = value;
    return array;
}
noInline(test);
noDFG(test);

for (var i = 0; i < 1e6; ++i)
    test(i);
