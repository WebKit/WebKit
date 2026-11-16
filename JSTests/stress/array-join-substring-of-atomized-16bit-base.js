function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected: ${JSON.stringify(expected)}`);
}

function test(separator) {
    // KELVIN SIGN lowercases to "k", so this registers a 16-bit atom "kkkk".
    const object = { ["KKKK".toLowerCase()]: 1 };
    const base = "k".repeat(4);
    const slice = base.slice(1);
    object[base];
    return [slice, "abc"].join(separator);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(""), "kkkabc");
    shouldBe(test(","), "kkk,abc");
}
