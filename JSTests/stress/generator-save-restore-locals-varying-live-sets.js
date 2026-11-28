function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* varying() {
    let a = 1, b = 2, c = 3, d = "d";
    yield a + b + c;
    a = yield b;
    c = yield c + a;
    b = yield d;
    d = yield a;
    return [a, b, c, d];
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = varying();
    shouldBe(iterator.next().value, 6);
    shouldBe(iterator.next(10).value, 2);
    shouldBe(iterator.next(20).value, 23);
    shouldBe(iterator.next(30).value, "d");
    shouldBe(iterator.next(40).value, 20);
    let result = iterator.next(50);
    shouldBe(result.done, true);
    shouldBe(JSON.stringify(result.value), '[20,40,30,50]');
}

function* stale() {
    let a = 1;
    let b = "b";
    yield a;
    a = yield b;
    b = yield a;
    a = "s" + a;
    yield b;
    return a + b;
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = stale();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, "b");
    shouldBe(iterator.next(2).value, 2);
    shouldBe(iterator.next(3).value, 3);
    let result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, "s23");
}

function* typesPerYield(count) {
    let value = 0;
    for (let i = 0; i < count; ++i) {
        value = yield value;
    }
    return value;
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = typesPerYield(5);
    shouldBe(iterator.next().value, 0);
    shouldBe(iterator.next(1).value, 1);
    shouldBe(iterator.next(1.5).value, 1.5);
    shouldBe(iterator.next("s").value, "s");
    shouldBe(iterator.next(null).value, null);
    let result = iterator.next({ });
    shouldBe(result.done, true);
    shouldBe(typeof result.value, "object");
}
