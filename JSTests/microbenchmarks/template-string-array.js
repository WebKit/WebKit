var array = [1, 2, 3];
function test()
{
    return `${array[0]}, ${array[1]}, ${array[2]}, ${array[0]}, ${array[1]}, ${array[2]}`;
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test();
