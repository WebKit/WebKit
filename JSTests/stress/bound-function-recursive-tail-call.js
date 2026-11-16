function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function sum(step, n, acc) {
    "use strict";
    if (!n)
        return acc + this.base;
    return boundSum(n - 1, acc + step);
}
noInline(sum);

function isEven(n) {
    "use strict";
    if (!n)
        return true;
    return boundIsOdd(n - 1);
}
noInline(isEven);

function isOdd(n) {
    "use strict";
    if (!n)
        return false;
    return isEven(n - 1);
}

function count(n, acc) {
    "use strict";
    if (!n)
        return acc;
    return boundCount(n - 1, acc + 1);
}

function callCount(n) {
    return count(n, 0) + 1;
}
noInline(callCount);

const boundSum = sum.bind({ base: 1000 }, 2);
const boundIsOdd = isOdd.bind(null);
const boundCount = count.bind(null);
const receiver = { base: 1 };

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(sum.call(receiver, 5, 0, 0), 1);
    shouldBe(sum.call(receiver, 5, 10, 0), 1023);
    shouldBe(isEven(10), true);
    shouldBe(isEven(11), false);
    shouldBe(callCount(10), 11);
}
