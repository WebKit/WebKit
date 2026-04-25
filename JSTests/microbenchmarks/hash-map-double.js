function test(map, i)
{
    return map.get(i);
}
noInline(test);

let map = new Map();
for (var i = 0; i < 1e6; ++i) {
    test(map, i * 0.253);
    test(map, i * 20.253);
    test(map, i * 3.2);
}
