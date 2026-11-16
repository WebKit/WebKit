function toIntegerOrInfinity(value) {
    let number = Number(value);
    if (isNaN(number))
        return 0;
    return Math.trunc(number) + 0;
}

function expected(size) {
    let n = toIntegerOrInfinity(size);
    return n === 1 || n === 2 || n === 4 || n === 8;
}

function check(size) {
    let actual = Atomics.isLockFree(size);
    let want = expected(size);
    if (actual !== want)
        throw new Error("Atomics.isLockFree(" + size + ") === " + actual + ", expected " + want);
}

function foo(size) {
    return Atomics.isLockFree(size);
}
noInline(foo);

let values = [
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12,
    1.9, 2.1, 4.9, 8.9,
    4294967296, 4294967297, 4294967298, 4294967300, 4294967304,
    -4294967295, -1, -0,
    NaN, Infinity, -Infinity,
    "1", "4", "4294967297",
    true, false,
    { valueOf() { return 4294967297; } },
    { valueOf() { return 1; } },
];

for (let i = 0; i < testLoopCount; ++i) {
    for (let value of values)
        check(value);
    if (foo(4294967297) !== false)
        throw new Error("bad JIT result for 4294967297");
    if (foo(1) !== true)
        throw new Error("bad JIT result for 1");
    if (foo(4) !== true)
        throw new Error("bad JIT result for 4");
    if (foo(8) !== true)
        throw new Error("bad JIT result for 8");
}
