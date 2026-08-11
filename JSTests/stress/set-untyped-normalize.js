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
let set = new Set();
for (let key of keys)
    set.add(key);

function test(set, key)
{
    return set.has(key);
}
noInline(test);

for (let i = 0; i < 1e4; ++i) {
    let j = 0;
    for (let key of keys) {
        shouldBe(test(set, key), true);
    }
}
shouldBe(test(set, 0.0), true);
