function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value) {
    return JSON.stringify(value, (key, value) => value);
}

let latin1 = ['x'.repeat(1200000), 'y'.repeat(100)];
let utf16 = ['x'.repeat(1200000), '\u3042'.repeat(100)];
let expectedLatin1 = reference(latin1);
let expectedUTF16 = reference(utf16);
for (let i = 0; i < 4; ++i) {
    shouldBe(JSON.stringify(latin1), expectedLatin1);
    shouldBe(JSON.stringify(utf16), expectedUTF16);
}
