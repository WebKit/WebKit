function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var keys = [
    "Cappuccino",
    -0.0,
    Symbol("Cocoa"),
    42,
    -42,
    null,
    undefined,
    420.5,
    0xffffffff,
    0x80000000,
    -1,
    -2147483648,
    {},
    [],
    false,
    true,
    NaN,
];

let i = 0;
let map = new Map();
for (let key of keys)
    map.set(key, i++);

function test(map, key)
{
    return map.get(key) + map.get(key);
}
noInline(test);

for (let i = 0; i < 1e4; ++i) {
    let j = 0;
    for (let key of keys) {
        let result = j + j;
        j++
        shouldBe(test(map, key), result);
    }
}
shouldBe(test(map, 0.0), 2);
