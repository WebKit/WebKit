function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(value) {
    return Iterator.from(value);
}
noInline(test);

function getIter() {
    let count = 0;
    return {
        next() {
            if (count < 5) {
                return { value: count++, done: false };
            }
            return { value: count, done: true };
        }
    };
}

for (let i = 0; i < testLoopCount; i++) {
    const result = test(getIter());
    shouldBe(result.next().value, 0);
    shouldBe(result.next().value, 1);
    shouldBe(result.next().value, 2);
    shouldBe(result.next().value, 3);
    shouldBe(result.next().value, 4);
}

for (let i = 0; i < testLoopCount; i++) {
    const result = test(getIter());
    const mapped = result.map((x) => x * 2);
    shouldBe(mapped.next().value, 0);
    shouldBe(mapped.next().value, 2);
    shouldBe(mapped.next().value, 4);
    shouldBe(mapped.next().value, 6);
    shouldBe(mapped.next().value, 8);
}
