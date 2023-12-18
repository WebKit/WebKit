var array = [];
for (var i = 0; i < 1000; ++i)
    array.push(i);

function test(array)
{
    return array.filter((value) => value < 1e5);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    test(array);
}
