function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* empty() {
    yield 1;
    yield 2;
}

function* partial(x) {
    yield 1;
    let a = x * 2;
    yield a;
    yield 3;
    return a;
}

function* onlyCaptured() {
    let captured = 1;
    const read = () => captured;
    yield read();
    captured = 2;
    yield read();
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = empty();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 2);
    shouldBe(iterator.next().done, true);

    iterator = partial(21);
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 42);
    shouldBe(iterator.next().value, 3);
    let result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, 42);

    iterator = onlyCaptured();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 2);
    shouldBe(iterator.next().done, true);
}
