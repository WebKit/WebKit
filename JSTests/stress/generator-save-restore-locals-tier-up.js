function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* counting(count, start) {
    let i = 0, accumulator = start, object = { f: 1 };
    while (i < count) {
        accumulator = accumulator + object.f;
        yield accumulator;
        ++i;
    }
    return accumulator;
}

function drive(start, count) {
    let iterator = counting(count, start);
    let last;
    for (let result = iterator.next(); !result.done; result = iterator.next())
        last = result.value;
    return last;
}

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(drive(0, 10), 10);
shouldBe(drive(0.5, 10), 10.5);
shouldBe(drive("s", 3), "s111");
shouldBe(drive(null, 2), 2);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(drive(0, 10), 10);
    shouldBe(drive(0.5, 10), 10.5);
    shouldBe(drive("s", 3), "s111");
    shouldBe(drive(null, 2), 2);
}

function* pair(count) {
    let a = 0, b = 0;
    for (let i = 0; i < count; ++i) {
        a = yield a;
        b = a;
    }
    return b;
}

function drivePair(values) {
    let iterator = pair(values.length);
    iterator.next();
    let last;
    for (let value of values)
        last = iterator.next(value).value;
    return last;
}

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(drivePair([1, 2, 3]), 3);
shouldBe(drivePair([1, 2, "x"]), "x");
shouldBe(drivePair([1, 2, 3.5]), 3.5);
shouldBe(drivePair([1, 2, undefined]), undefined);
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(drivePair([1, 2, 3]), 3);
    shouldBe(drivePair([1, 2, "x"]), "x");
    shouldBe(drivePair([1, 2, 3.5]), 3.5);
    shouldBe(drivePair([1, 2, undefined]), undefined);
}
