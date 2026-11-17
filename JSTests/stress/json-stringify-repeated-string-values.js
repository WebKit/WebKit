function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + String(actual).slice(0, 200) + ' expected: ' + String(expected).slice(0, 200));
}

function reference(value, space) {
    return JSON.stringify(value, (key, value) => value, space);
}

let latin1Strings = [
    '', 'a', 'ab', 'abc', 'abcd', 'abcdefg', 'abcdefgh', 'abcdefghijklmno', 'abcdefghijklmnop', 'abcdefghijklmnopq',
    'x'.repeat(31), 'y'.repeat(32), 'z'.repeat(33), 'https://browserbench.org/Speedometer2.0/resources/benchmark-runner.js',
    'with "quote"', 'with \\ backslash', 'line\nbreak', '\x01control', 'caf\xe9', 'w'.repeat(100),
];
let mixedStrings = [...latin1Strings, 'ሴ wide'];

function makeRopes(strings) {
    let ropes = [];
    for (let i = 0; i < 10; ++i)
        ropes.push('rope-' + String(i).repeat(i) + '-' + strings[i % strings.length]);
    return ropes;
}

function makeObjects(strings, count) {
    let ropes = makeRopes(strings);
    let objects = [];
    for (let i = 0; i < count; ++i) {
        objects.push({
            a: strings[i % strings.length],
            b: i,
            c: strings[(i * 7) % strings.length],
            d: [strings[i % 3], ropes[i % ropes.length]],
            e: { f: strings[(i * 3) % strings.length], g: ropes[(i * 5) % ropes.length] },
            h: ropes[i % ropes.length],
        });
    }
    return objects;
}

function test(objects) {
    shouldBe(JSON.stringify(objects), reference(objects));
    shouldBe(JSON.stringify(objects, null, 2), reference(objects, 2));
    shouldBe(JSON.stringify({ wrapper: objects }), reference({ wrapper: objects }));
}

for (let strings of [latin1Strings, mixedStrings]) {
    for (let count of [1, 10, 100, 1000])
        test(makeObjects(strings, count));
}

// Repeated outputs larger than the static buffer select the dynamic Latin-1 buffer, which caches strings it has written.
for (let strings of [latin1Strings, mixedStrings]) {
    let objects = makeObjects(strings, 1000);
    let expected = reference(objects);
    for (let i = 0; i < 5; ++i)
        shouldBe(JSON.stringify(objects), expected);
}
