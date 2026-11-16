function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad: ' + actual + ' vs ' + expected);
}
noInline(shouldBe);

function sumMapOrSet(iterable) {
    let sum = 0;
    for (const e of iterable) {
        if (typeof e === "number")
            sum += e;
        else
            sum += e[0] + e[1];
    }
    return sum;
}
noInline(sumMapOrSet);

let map = new Map([[1, 10], [2, 20], [3, 30]]);
let set = new Set([100, 200, 300]);

for (let i = 0; i < 80; ++i) {
    if (i & 1)
        shouldBe(sumMapOrSet(map), 66);
    else
        shouldBe(sumMapOrSet(set), 600);
}

let originalSetIterator = Set.prototype[Symbol.iterator];
Set.prototype[Symbol.iterator] = function () { return originalSetIterator.call(this); };

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(sumMapOrSet(map), 66);
