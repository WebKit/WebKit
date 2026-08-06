function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function makeEntries(count, distinct, prefix) {
    let entries = [];
    for (let i = 0; i < count; ++i) {
        let key = prefix + String.fromCharCode(0x41 + ((i * 7) % distinct));
        entries.push({ id: i, key, toString() { return this.key; } });
    }
    return entries;
}

function verifyStable(entries) {
    let expected = entries.slice().sort((a, b) => a.key < b.key ? -1 : a.key > b.key ? 1 : a.id - b.id);
    let actual = entries.slice().sort();
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < actual.length; ++i)
        shouldBe(actual[i].id, expected[i].id);
}

for (let count = 20; count <= 40; ++count) {
    for (let distinct of [1, 2, 3, 5])
        verifyStable(makeEntries(count, distinct, ""));
}

for (let count of [40, 100])
    verifyStable(makeEntries(count, 3, "0123456789012345678901234567890123456789"));
