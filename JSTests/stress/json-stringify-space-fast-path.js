// Differential test: fast path JSON.stringify(v, null, space) must match the
// general Stringifier, which we force via an identity callable replacer.
let failures = 0;

const values = [
    {},
    [],
    { a: 1 },
    [1, 2, 3],
    [[]],
    [{}],
    { a: {} },
    { a: [] },
    { a: { b: { c: [1, [2, [3, {}]]] } } },
    { id: 1, name: 'user1', tags: ['alpha', 'beta'], nested: { active: true, score: 1.5, items: [1, 2, 3] } },
    { s: 'esc"ape\\\n\t', t: 'plain' },
    { u: undefined, v: 1 },
    { u: undefined },
    [undefined, null, true, false],
    [function () {}, Symbol('x'), undefined],
    { f: function () {}, sym: Symbol('x'), ok: 1 },
    'top-level string',
    42,
    1.25,
    -0,
    NaN,
    Infinity,
    true,
    null,
    [1e21, 1e-7, 0.1, -2147483648, 2147483647],
    { 'k"ey': 1 },
    { '': 2 },
    { 'ユニコード': 'マルチバイト文字列' },
    ['16bit→string', { a: '€' }],
    [[[[[[[[[[1]]]]]]]]]],
    new Date(0),
    { d: new Date(0) },
    [0.5, [0.25, { x: 0.125 }]],
];

const spaces = [
    undefined, null, 2, 0, -1, 1, 10, 11, 100, 2.7, 0.5, -2.5, NaN, Infinity, -Infinity,
    '\t', '  ', 'ab', '', '0123456789ABC', '', '€', 'あ',
    true, false, {}, [], new Number(3), new String('xy'), Symbol('s'), 10n, 3n,
];

function show(x) {
    if (typeof x === 'symbol') return 'Symbol';
    if (typeof x === 'bigint') return x + 'n';
    try { return JSON.stringify(x) ?? String(x); } catch { return String(x); }
}

for (const space of spaces) {
    for (const value of values) {
        let fast, slow;
        let fastErr, slowErr;
        try { fast = JSON.stringify(value, null, space); } catch (e) { fastErr = String(e); }
        try { slow = JSON.stringify(value, (k, x) => x, space); } catch (e) { slowErr = String(e); }
        if (fast !== slow || fastErr !== slowErr) {
            failures++;
            print('MISMATCH space=' + show(space) + ' value=' + show(value));
            print('  fast: ' + (fastErr ?? JSON.stringify(fast)));
            print('  slow: ' + (slowErr ?? JSON.stringify(slow)));
        }
    }
}

// Large outputs: force StaticBuffer overflow into DynamicBuffer with gap.
{
    const big = [];
    for (let i = 0; i < 2000; i++)
        big.push({ idx: i, name: 'n' + i, vals: [i, i + 1, i + 2], deep: { x: i * 0.5 } });
    for (const space of [2, '\t', 10]) {
        const fast = JSON.stringify(big, null, space);
        const slow = JSON.stringify(big, (k, x) => x, space);
        if (fast !== slow) {
            failures++;
            print('MISMATCH big space=' + show(space) + ' lens ' + fast.length + ' vs ' + slow.length);
        }
    }
}

// 16-bit content with gap (char16_t fast path).
{
    const v = { 'キー': ['値', { nested: '🎉 emoji', n: 1 }] };
    for (const space of [2, '\t', 4]) {
        const fast = JSON.stringify(v, null, space);
        const slow = JSON.stringify(v, (k, x) => x, space);
        if (fast !== slow) {
            failures++;
            print('MISMATCH 16bit space=' + show(space));
            print('  fast: ' + fast);
            print('  slow: ' + slow);
        }
    }
}

// Deep nesting with gap.
{
    let v = 1;
    for (let i = 0; i < 200; i++) v = [v];
    const fast = JSON.stringify(v, null, 2);
    const slow = JSON.stringify(v, (k, x) => x, 2);
    if (fast !== slow) { failures++; print('MISMATCH deep nesting'); }
}

if (failures)
    throw new Error('FAILED: ' + failures + ' mismatches');
print('all ok');
