function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* forIn(object) {
    let count = 0;
    for (let key in object) {
        count += 1;
        yield key + count;
    }
    return count;
}

function* forOf(iterable) {
    let sum = 0;
    for (let [key, value] of iterable) {
        sum += value;
        yield key + sum;
    }
    return sum;
}

function* destructure() {
    let { x, y = 2, ...rest } = yield 0;
    let [first, ...others] = yield [x, y, Object.keys(rest).join("")];
    return first + others.length;
}

function* withScope(object) {
    with (object) {
        yield a;
        b = a + (yield b);
        yield b;
    }
    return object.b;
}

function* spreadArguments(...values) {
    let copy = [...values];
    for (let value of values)
        copy.push(yield value);
    return copy.length;
}

for (let i = 0; i < testLoopCount; ++i) {
    let iterator = forIn({ p: 1, q: 2, r: 3 });
    shouldBe(iterator.next().value, "p1");
    shouldBe(iterator.next().value, "q2");
    shouldBe(iterator.next().value, "r3");
    let result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, 3);

    iterator = forOf(new Map([["a", 1], ["b", 2]]));
    shouldBe(iterator.next().value, "a1");
    shouldBe(iterator.next().value, "b3");
    result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, 3);

    iterator = destructure();
    shouldBe(iterator.next().value, 0);
    shouldBe(JSON.stringify(iterator.next({ x: 1, z: 3, w: 4 }).value), '[1,2,"zw"]');
    result = iterator.next([10, 20, 30]);
    shouldBe(result.done, true);
    shouldBe(result.value, 12);

    let object = { a: 1, b: 2 };
    iterator = withScope(object);
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 2);
    shouldBe(iterator.next(10).value, 11);
    result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, 11);
    shouldBe(object.b, 11);

    iterator = spreadArguments(1, 2, 3);
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next("a").value, 2);
    shouldBe(iterator.next("b").value, 3);
    result = iterator.next("c");
    shouldBe(result.done, true);
    shouldBe(result.value, 6);
}
