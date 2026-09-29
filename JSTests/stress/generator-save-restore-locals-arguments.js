function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* args(a, b = a + 1, ...rest) {
    let local = arguments.length;
    yield local;
    local += a + b;
    yield local;
    local += rest.length;
    yield local;
    return this.value + local;
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = args.call({ value: 100 }, 1, undefined, 3, 4);
    shouldBe(iterator.next().value, 4);
    shouldBe(iterator.next().value, 7);
    shouldBe(iterator.next().value, 9);
    let result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, 109);
}
