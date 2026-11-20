function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value) {
    return JSON.stringify(value, (key, value) => value);
}

let padding = 'line\n"\u00e9"\u3042\u3044 '.repeat(40);
function makeObjects(count, salt) {
    let objects = [];
    for (let i = 0; i < count; ++i)
        objects.push({ id: i + salt, name: '\u3042\u3044' + (i % 17), text: padding.slice(i % 40), children: [i, 'x'.repeat(i % 40)] });
    return objects;
}

for (let [count, salt] of [[50, 0], [1000, 1], [2000, 2], [330, 3], [2000, 4]]) {
    let objects = makeObjects(count, salt);
    let expected = reference(objects);
    for (let i = 0; i < 2; ++i)
        shouldBe(JSON.stringify(objects), expected);
}

let wide = '\u3042'.repeat(30000);
for (let fillerLength of [100000, 600000, 1100000]) {
    let value = ['x'.repeat(fillerLength), wide, 'line\n"quoted"\\'.repeat(2000), wide];
    shouldBe(JSON.stringify(value), reference(value));
}
