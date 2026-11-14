function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value, space) {
    return JSON.stringify(value, (key, value) => value, space);
}

let strings = [
    '', 'a', 'ab', 'abc', 'abcd', 'abcdefg', 'abcdefgh', 'abcdefghijklmno', 'abcdefghijklmnop', 'abcdefghijklmnopq',
    'x'.repeat(31), 'y'.repeat(32), 'z'.repeat(33), 'https://browserbench.org/Speedometer2.0/resources/benchmark-runner.js',
    'with "quote"', 'with \\ backslash', 'line\nbreak', '\x01control', 'caf\xe9', '\u1234 wide', 'w'.repeat(100),
];
let ropes = [];
for (let i = 0; i < 10; ++i)
    ropes.push('rope-' + String(i).repeat(i) + '-' + strings[i % strings.length]);

function makeObjects(count) {
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

for (let count of [1, 10, 100, 1000, 5000]) {
    let objects = makeObjects(count);
    shouldBe(JSON.stringify(objects), reference(objects));
    shouldBe(JSON.stringify(objects, null, 2), reference(objects, 2));
    shouldBe(JSON.stringify({ wrapper: objects }), reference({ wrapper: objects }));
}

let objects = makeObjects(3000);
let first = JSON.stringify(objects);
for (let i = 0; i < 5; ++i)
    shouldBe(JSON.stringify(objects), first);
