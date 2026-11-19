function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual.slice(0, 200) + ' expected: ' + expected.slice(0, 200));
}

function reference(value, space) {
    return JSON.stringify(value, (key, value) => value, space);
}

function check(value) {
    shouldBe(JSON.stringify(value), reference(value));
    shouldBe(JSON.stringify(value, null, 2), reference(value, 2));
    shouldBe(JSON.stringify({ wrapped: value }), reference({ wrapped: value }));
}

let mixed = [];
for (let i = 0; i < 1000; ++i) {
    switch (i % 11) {
    case 0: mixed.push({ a: i, b: 'x' }); break;
    case 1: mixed.push({ a: i, b: 'x' }); break;
    case 2: mixed.push({ c: [i, { a: i, b: 'y' }], d: {} }); break;
    case 3: mixed.push(i); break;
    case 4: mixed.push('string ' + (i % 5)); break;
    case 5: mixed.push(null); break;
    case 6: mixed.push([{ a: i, b: 'z' }, { a: i + 1, b: 'z' }]); break;
    case 7: mixed.push({ b: 'reordered', a: i }); break;
    case 8: mixed.push({}); break;
    case 9: mixed.push(i % 2 ? true : 1.5); break;
    case 10: mixed.push({ a: undefined, b: i }); break;
    }
}
check(mixed);

let uniform = [];
for (let i = 0; i < 1000; ++i)
    uniform.push({ id: i, name: 'n' + (i % 13), nested: { x: i, y: [i, i + 1] }, list: [{ id: i }] });
check(uniform);

let withHole = [{ a: 1 }, { a: 2 }];
withHole[3] = { a: 4 };
check(withHole);

let withUndefinedElement = [{ a: 1 }, undefined, { a: 2 }];
check(withUndefinedElement);

let withFunction = [{ a: 1 }, function () { }, { a: 2 }];
check(withFunction);
