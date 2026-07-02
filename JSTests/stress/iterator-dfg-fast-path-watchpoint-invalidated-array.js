function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad: ' + actual + ' vs ' + expected);
}
noInline(shouldBe);

function sumArrayOrMap(iterable) {
    let sum = 0;
    for (const e of iterable) {
        if (typeof e === "number")
            sum += e;
        else
            sum += e[0] + e[1];
    }
    return sum;
}
noInline(sumArrayOrMap);

let arr = [1, 2, 3, 4, 5];
let map = new Map([[1, 10], [2, 20], [3, 30]]);

for (let i = 0; i < 80; ++i) {
    if (i & 1)
        shouldBe(sumArrayOrMap(arr), 15);
    else
        shouldBe(sumArrayOrMap(map), 66);
}

let originalArrayIterator = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function () { return originalArrayIterator.call(this); };

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(sumArrayOrMap(map), 66);
