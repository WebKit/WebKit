function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
}

function wide(text) {
    return $vm.make16BitStringIfPossible(text);
}

// Replaces each character with one that has the same low byte and a nonzero high byte.
function alias(text, index) {
    return text.slice(0, index) + String.fromCharCode(text.charCodeAt(index) | 0x100) + text.slice(index + 1);
}

function checkKeys(object, keys, values) {
    const actual = Object.keys(object);
    shouldBe(actual.length, keys.length);
    for (let i = 0; i < keys.length; ++i) {
        shouldBe(actual[i], keys[i]);
        shouldBe(object[keys[i]], values[i]);
    }
}

const padding = " ".repeat(40);
const names = [
    "",
    "a",
    "id",
    "title",
    "completed",
    "x".repeat(29),
    "y".repeat(30),
    "été",
    "あ",
    "電子",
];

const iterations = Math.min(testLoopCount, 50);
for (let iteration = 0; iteration < iterations; ++iteration) {
    for (const name of names) {
        const keys = [name + "p", name + "q", name + "r"];
        const object = { [keys[0]]: 1, [keys[1]]: "v", [keys[2]]: null };
        const text = JSON.stringify(object);
        for (const source of [text + padding, wide(text + padding), wide(text), text])
            checkKeys(JSON.parse(source), keys, [1, "v", null]);

        // A Structure with several transitions goes through the prefixed table.
        const other = [name + "p", name + "s"];
        checkKeys(JSON.parse(wide(`{"${other[0]}":1,"${other[1]}":2}${padding}`)), other, [1, 2]);

        // Same low bytes as a recorded key, different high bytes, at every position of the key's text.
        for (let i = 0; i < keys[0].length; ++i) {
            const aliased = [alias(keys[0], i), keys[1]];
            checkKeys(JSON.parse(wide(`{"${aliased[0]}":1,"${aliased[1]}":2}${padding}`)), aliased, [1, 2]);
        }
    }

    // A key followed by characters whose low bytes are a quote and a colon.
    JSON.parse(wide(`{"title":1,"done":2}${padding}`));
    const quoteAlias = ["titleĢĺ", "done"];
    checkKeys(JSON.parse(wide(`{"${quoteAlias[0]}":1,"done":2}${padding}`)), quoteAlias, [1, 2]);
    const prefixAlias = ["Ŵitle", "done"];
    checkKeys(JSON.parse(wide(`{"${prefixAlias[0]}":1,"done":2}${padding}`)), prefixAlias, [1, 2]);

    const escaped = JSON.parse(wide(`{"tit\\u006ce":1,"done":2}${padding}`));
    checkKeys(escaped, ["title", "done"], [1, 2]);

    if (iteration % 10 === 0)
        gc();
}
