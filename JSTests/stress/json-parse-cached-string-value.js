function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

const padding = " ".repeat(40);
const values = [
    "",
    "a",
    "ab",
    "click",
    "0.12",
    "0.15",
    "x".repeat(29),
    "y".repeat(30),
    "z".repeat(31),
    "été",
    "a\"b",
    "a\\b",
    "line\nbreak",
    "あ",
];

const iterations = Math.min(testLoopCount, 50);
for (let iteration = 0; iteration < iterations; ++iteration) {
    for (let i = 0; i < values.length; ++i) {
        for (const other of [values[i], values[(i + iteration) % values.length]]) {
            const object = { key: other, next: 1 };
            const text = JSON.stringify(object);
            const parsed = JSON.parse(text + padding);
            shouldBe(parsed.key, other);
            shouldBe(parsed.next, 1);
            shouldBe(JSON.parse(text).key, other);
        }

        const prefixed = JSON.parse(`{"key":${JSON.stringify(values[i] + "q")},"next":1}${padding}`);
        shouldBe(prefixed.key, values[i] + "q");
    }

    const escaped = JSON.parse(`{"key":"cl\\u0069ck","next":1}${padding}`);
    shouldBe(escaped.key, "click");
    const unescaped = JSON.parse(`{"key":"click","next":1}${padding}`);
    shouldBe(unescaped.key, "click");

    if (iteration % 10 === 0)
        gc();
}
