function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* handlers() {
    let a = 1, b = 2;
    try {
        a = yield a;
        throw new Error("inner");
    } catch (error) {
        b = yield b + a + error.message.length;
    } finally {
        a = yield a + b;
    }
    return [a, b];
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = handlers();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next(10).value, 17);
    shouldBe(iterator.next(20).value, 30);
    let result = iterator.next(30);
    shouldBe(result.done, true);
    shouldBe(JSON.stringify(result.value), "[30,20]");
}

function* finallyYields() {
    let a = "a", b = "b";
    try {
        a += yield 1;
        b += yield 2;
    } finally {
        a += yield 3;
        b += yield 4;
    }
    return a + b;
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = finallyYields();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.return("r").value, 3);
    shouldBe(iterator.next("x").value, 4);
    let result = iterator.next("y");
    shouldBe(result.done, true);
    shouldBe(result.value, "r");

    iterator = finallyYields();
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next("1").value, 2);
    shouldBe(iterator.throw(new Error("t")).value, 3);
    shouldBe(iterator.next("x").value, 4);
    let thrown = null;
    try {
        iterator.next("y");
    } catch (error) {
        thrown = error;
    }
    shouldBe(thrown.message, "t");
}

function* rethrow() {
    let count = 0;
    while (true) {
        try {
            count += yield count;
        } catch (error) {
            count += error;
            if (count > 100)
                throw new Error(`count ${count}`);
        }
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = rethrow();
    shouldBe(iterator.next().value, 0);
    shouldBe(iterator.next(1).value, 1);
    shouldBe(iterator.throw(50).value, 51);
    shouldBe(iterator.next(1).value, 52);
    let thrown = null;
    try {
        iterator.throw(50);
    } catch (error) {
        thrown = error;
    }
    shouldBe(thrown.message, "count 102");
    shouldBe(iterator.next().done, true);
}
