function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

let values = [];
for (let power = 1; power <= 1000000000; power *= 10) {
    for (let delta of [-1, 0, 1])
        values.push(power + delta, -(power + delta));
}
values.push(0, 2147483647, -2147483647, -2147483648, 12345, -12345, 123456789, -123456789);
let seed = 1;
for (let i = 0; i < 2000; ++i) {
    seed = (seed * 1103515245 + 12345) | 0;
    values.push(seed >> (i % 31));
}

function check(values) {
    shouldBe(JSON.stringify(values), '[' + values.map(String).join(',') + ']');

    let object = {};
    for (let i = 0; i < values.length; ++i)
        object['k' + i] = values[i];
    shouldBe(JSON.stringify(object), '{' + values.map((value, i) => '"k' + i + '":' + value).join(',') + '}');

    let objects = values.map(value => ({ a: value, b: 's', c: value }));
    let expected = '[' + values.map(value => '{"a":' + value + ',"b":"s","c":' + value + '}').join(',') + ']';
    shouldBe(JSON.stringify(objects), expected);
    shouldBe(JSON.stringify(JSON.parse(JSON.stringify(objects, null, 2))), expected);

    let wide = values.map(value => ({ a: value, b: '\u1234', c: value }));
    shouldBe(JSON.stringify(wide), '[' + values.map(value => '{"a":' + value + ',"b":"\u1234","c":' + value + '}').join(',') + ']');
}

check(values);
for (let i = 0; i < values.length; i += 50)
    check(values.slice(i, i + 50));
