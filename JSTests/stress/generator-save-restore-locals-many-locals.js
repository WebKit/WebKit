function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function makeWide(count) {
    let declarations = [];
    let names = [];
    for (let i = 0; i < count; ++i) {
        declarations.push(`let v${i} = ${i};`);
        names.push(`v${i}`);
    }
    return new Function(`
        return function* wide() {
            ${declarations.join("\n")}
            yield 0;
            ${names.map((name) => `${name} += 1;`).join("\n")}
            yield 1;
            ${names.map((name, index) => index % 2 ? `${name} = ${name} + 0.5;` : "").join("\n")}
            yield 2;
            return ${names.join(" + ")};
        }`)();
}

function check(count, rounds) {
    let wide = makeWide(count);
    let expected = 0;
    for (let i = 0; i < count; ++i)
        expected += i + 1 + (i % 2 ? 0.5 : 0);
    for (let round = 0; round < rounds; ++round) {
        let iterator = wide();
        shouldBe(iterator.next().value, 0);
        shouldBe(iterator.next().value, 1);
        shouldBe(iterator.next().value, 2);
        let result = iterator.next();
        shouldBe(result.done, true);
        shouldBe(result.value, expected);
    }
}

for (let count of [1, 2, 3, 9, 31, 32, 33, 62, 63, 64, 65, 70, 127, 128, 129, 300])
    check(count, 3);
check(64, testLoopCount);
