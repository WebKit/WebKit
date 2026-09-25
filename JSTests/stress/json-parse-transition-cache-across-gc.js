function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

// Each round builds Structures reachable only from its own result, drops them, and collects, so later
// rounds reuse their memory for Structures with the same key names in a different order.
for (let round = 0; round < 60; ++round) {
    let names = [];
    for (let i = 0; i < 8; ++i)
        names.push(`k${(round * 7 + i * 3) % 11}_${i % 3}`);
    names = [...new Set(names)];
    if (round & 1)
        names.reverse();

    let parts = [];
    for (let i = 0; i < 64; ++i) {
        let count = 1 + (i % names.length);
        let fields = [];
        for (let j = 0; j < count; ++j)
            fields.push(`"${names[(i + j) % names.length]}":${round * 1000 + i * 10 + j}`);
        parts.push(`{${fields.join(",")}}`);
    }
    let source = `[${parts.join(",")}]`;

    let result = JSON.parse(source);
    for (let i = 0; i < result.length; ++i) {
        let count = 1 + (i % names.length);
        let object = result[i];
        let keys = Object.keys(object);
        shouldBe(keys.length, count);
        for (let j = 0; j < count; ++j) {
            shouldBe(keys[j], names[(i + j) % names.length]);
            shouldBe(object[keys[j]], round * 1000 + i * 10 + j);
        }
    }
    result = null;

    if (round % 3 === 0)
        fullGC();
    else if (round % 3 === 1)
        edenGC();
}
