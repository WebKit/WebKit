function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function* gen() {
    let limit = 1000;
    while (limit-- > 0) {
        yield limit;
    }
}

for (let i = 0; i < 1e4; i++) {
    const iter = gen();
    const result = iter.map(e => e * 2).toArray();
    shouldBe(result.length, 1000);
}
