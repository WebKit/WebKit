const map = new Map();
for (let i = 0; i < 32; i++)
    map.set(i, i * 2);

function test(map) {
    return map.entries().toArray();
}
noInline(test);

for (let i = 0; i < 1e6; i++)
    test(map);
