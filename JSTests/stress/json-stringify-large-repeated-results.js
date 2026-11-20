function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value) {
    return JSON.stringify(value, (key, value) => value);
}

let padding = 'https://example.com/resource/'.repeat(32);
function makeObjects(count, salt) {
    let objects = [];
    for (let i = 0; i < count; ++i)
        objects.push({ id: i + salt, name: 'name' + (i % 17), url: padding.slice(i % 40) + (i % 5) + '/resource.js', position: { line: i, column: i % 80 }, children: [] });
    return objects;
}

for (let [count, salt] of [[1100, 0], [1100, 1], [2200, 2], [800, 3], [1600, 4], [280, 5], [1600, 6]]) {
    let objects = makeObjects(count, salt);
    let expected = reference(objects);
    for (let i = 0; i < 3; ++i)
        shouldBe(JSON.stringify(objects), expected);
}
