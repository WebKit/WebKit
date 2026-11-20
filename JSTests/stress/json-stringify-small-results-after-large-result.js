function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value) {
    return JSON.stringify(value, (key, value) => value);
}

let large = ['x'.repeat(1200000)];
let smalls = [{ a: 1, b: 'small' }, 'y'.repeat(100), [1, 2, 3], 'z'.repeat(2000), { s: 'w'.repeat(9000) }];
let withToJSON = [{ toJSON() { return 'from toJSON'; } }, 'v'.repeat(10000)];

let expectedLarge = reference(large);
let expectedSmalls = smalls.map(value => reference(value));
let expectedWithToJSON = reference(withToJSON);

for (let i = 0; i < 4; ++i) {
    shouldBe(JSON.stringify(large), expectedLarge);
    for (let j = 0; j < smalls.length; ++j)
        shouldBe(JSON.stringify(smalls[j]), expectedSmalls[j]);
    shouldBe(JSON.stringify(large), expectedLarge);
    shouldBe(JSON.stringify(large), expectedLarge);
    shouldBe(JSON.stringify(withToJSON), expectedWithToJSON);
}
