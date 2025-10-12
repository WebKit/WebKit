function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function* gen() {
    let limit = 1024;
    while (--limit > 0) {
        yield limit;
    }
}

for (let i = 0; i < 1e4; i++) {
    const iter = gen();
    const result = new Uint8Array(iter);
    shouldBe(result.length, 1023);
}

