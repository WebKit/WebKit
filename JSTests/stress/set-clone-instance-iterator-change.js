function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let set = new Set([0, 1, 2, 3, 4]);
for (let i = 0; i < 1e2; ++i) {
    let cloned = new Set(set);
    shouldBe(cloned.size, set.size);
}

set[Symbol.iterator] = function () { return [][Symbol.iterator](); };
for (let i = 0; i < 1e2; ++i) {
    let cloned = new Set(set);
    shouldBe(cloned.size, 0);
}
