const map = new Map();
for (let i = 0; i < 10; ++i)
    map.set(i, i * 2);

function test(map) {
    let sum = 0;
    for (let [k, v] of map)
        sum += k + v;
    return sum;
}
noInline(test);

for (let i = 0; i < 1e5; ++i)
    test(map);
