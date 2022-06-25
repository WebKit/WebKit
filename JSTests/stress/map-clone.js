function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let map = new Map();
for (let i = 0; i < 5; ++i)
    map.set(i, i);

for (let i = 0; i < 1e2; ++i) {
    let cloned = new Map(map);
    shouldBe(cloned.size, map.size);
}

Map.prototype.set = function empty(value) { };
for (let i = 0; i < 1e2; ++i) {
    let cloned = new Map(map);
    shouldBe(cloned.size, 0);
}
