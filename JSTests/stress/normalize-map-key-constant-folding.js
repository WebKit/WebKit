function test(i)
{
    var map = new Map();
    var key = "Hello";
    if (i & 0x1)
        key = 42;
    map.set(key, 42);
    return map;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(i);
