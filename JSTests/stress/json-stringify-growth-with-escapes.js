function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value) {
    return JSON.stringify(value, (key, value) => value);
}

let escaped = 'line\n"quoted"\\'.repeat(5000);
for (let fillerLength of [100000, 500000, 1000000, 1100000]) {
    let filler = 'x'.repeat(fillerLength);
    let values = [
        [filler, escaped],
        [filler, escaped, filler],
        { a: filler, b: escaped, c: [escaped, filler] },
        [filler.slice(1), 'end\u0001', escaped],
    ];
    for (let value of values)
        shouldBe(JSON.stringify(value), reference(value));
}
