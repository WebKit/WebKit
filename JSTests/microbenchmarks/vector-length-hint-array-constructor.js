function test(constructor)
{
    var array = new constructor(1, 2);
    for (var i = 0; i < 20; ++i)
        array.push(i);
    return array;
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    var result = test(Array);
