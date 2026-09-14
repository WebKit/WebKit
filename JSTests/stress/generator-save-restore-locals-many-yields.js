function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function makeManyYields(count) {
    let body = [];
    for (let i = 0; i < count; ++i)
        body.push(`a += yield ${i}; b = "${i % 3}" + b;`);
    return new Function(`
        return function* manyYields() {
            let a = 0, b = "";
            ${body.join("\n")}
            return [a, b.length];
        }`)();
}

function check(count, rounds) {
    let manyYields = makeManyYields(count);
    for (let round = 0; round < rounds; ++round) {
        let iterator = manyYields();
        shouldBe(iterator.next().value, 0);
        for (let i = 1; i < count; ++i)
            shouldBe(iterator.next(i).value, i);
        let result = iterator.next(count);
        shouldBe(result.done, true);
        shouldBe(result.value[0], count * (count + 1) / 2);
        shouldBe(result.value[1], count);
    }
}

for (let count of [1, 2, 63, 64, 65, 300])
    check(count, 3);
check(64, testLoopCount);
