function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function shouldThrow(func, errorMessage) {
    let error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown");
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function* declaredAfterYield(flag) {
    yield 1;
    if (flag)
        return later;
    let later = 5;
    yield later;
    {
        yield 2;
        let x = 9;
        yield x;
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = declaredAfterYield(false);
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 5);
    shouldBe(iterator.next().value, 2);
    shouldBe(iterator.next().value, 9);
    shouldBe(iterator.next().done, true);
}
shouldThrow(() => {
    let iterator = declaredAfterYield(true);
    iterator.next();
    iterator.next();
}, "ReferenceError: Cannot access 'later' before initialization.");

function* switchTDZ(flag) {
    switch (flag) {
    case true:
        let x = 42;
        yield x;
    case false:
        yield 1;
        return x;
    }
}

function drive(flag) {
    let iterator = switchTDZ(flag);
    let last;
    for (let result = iterator.next(); !result.done; result = iterator.next())
        last = result.value;
    return last;
}

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(drive(true), 1);
for (let i = 0; i < 10; ++i)
    shouldThrow(() => drive(false), "ReferenceError: Cannot access 'x' before initialization.");
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(drive(true), 1);
    if (!(i % 100))
        shouldThrow(() => drive(false), "ReferenceError: Cannot access 'x' before initialization.");
}
