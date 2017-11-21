function test()
{
    let map = new Map();
    for (let i = 0; i < 1e6; ++i) {
        if (!map.has(i))
            map.set(i, i);
    }
    return map
}
noInline(test);

test();
