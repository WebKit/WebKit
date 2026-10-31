// Differential test for JSON.stringify's fast path on outputs larger than its static buffer (8192 characters), which
// use the dynamic buffer, the Structure cache with its larger tables, and the hint of recent result lengths. The
// reference output comes from the general Stringifier, which we force with an identity replacer.

const spaces = [undefined, 2];

function check(name, value) {
    for (const space of spaces) {
        const fast = JSON.stringify(value, null, space);
        const slow = JSON.stringify(value, (key, x) => x, space);
        if (fast !== slow) {
            let index = 0;
            while (index < fast.length && fast[index] === slow[index])
                ++index;
            throw new Error(`${name}: mismatch with space ${JSON.stringify(space)} at ${index} (lengths ${fast.length} and ${slow.length})\n fast: ${fast.slice(Math.max(0, index - 40), index + 40)}\n slow: ${slow.slice(Math.max(0, index - 40), index + 40)}`);
        }
    }
}

function makeBig(count, tail) {
    const rows = [];
    for (let i = 0; i < count; i++)
        rows.push({ id: i, name: 'item' + i, price: i + 0.25, tags: ['a', 'b'], flag: i % 2 === 0, extra: i % 7 === 0 ? undefined : { x: i } });
    if (tail !== undefined)
        rows.push(tail);
    return rows;
}

const bigCount = 120;

// Two large results in a row make the next stringify try the dynamic buffer first.
function armDynamicBufferFirst() {
    for (let i = 0; i < 2; i++) {
        if (JSON.stringify(makeBig(bigCount)).length <= 8192)
            throw new Error("Output should be larger than the static buffer");
    }
}

check('big', makeBig(bigCount));
check('big again', makeBig(bigCount));
armDynamicBufferFirst();
check('small after big', makeBig(5));
check('small after small', makeBig(3));
armDynamicBufferFirst();
check('big after big', makeBig(bigCount));

// The fast path gives up after more than the static buffer's worth of output, which must still produce the right
// result, and must not keep trying the dynamic buffer first for the calls after it.
const tails = {
    '16-bit tail': { id: -1, name: '中文' },
    'Date tail': { id: -1, d: new Date(0) },
    'function tail': function () { },
    'toJSON tail': { toJSON() { return { id: -1 }; } },
    'symbol key tail': { [Symbol('s')]: 1, id: -1 },
    'escaped key tail': { 'k"ey': 1 },
};

for (const name in tails) {
    armDynamicBufferFirst();
    check(`big with ${name}`, makeBig(bigCount, tails[name]));
    check(`small after big with ${name}`, makeBig(4));
    check(`big after big with ${name}`, makeBig(bigCount));
}

armDynamicBufferFirst();
check('big with 16-bit head', [{ id: -1, name: '中文' }, ...makeBig(bigCount)]);
check('small after big with 16-bit head', makeBig(4));

// Nested wide objects fill the scratch space for recording Structures on the first visit.
{
    function makeNestedWide(level, depth, seed) {
        const object = {};
        for (let i = 0; i < 31; i++)
            object[`l${level}_${i}`] = i % 2 ? seed + i : 'v' + seed + '_' + i;
        object[`l${level}_last`] = depth > 1 ? makeNestedWide(level + 1, depth - 1, seed + 1) : seed;
        return object;
    }
    const trees = [];
    for (let seed = 0; seed < 12; seed++)
        trees.push(makeNestedWide(0, 3, seed));
    check('big nested wide', trees);
    check('big nested wide 16-bit', [...trees, '中']);
}

// Many properties in large output, so the dynamic buffer caches Structures the static buffer would not.
{
    function makeWide(count, seed) {
        const object = {};
        for (let i = 0; i < count; i++)
            object['p' + i] = i % 3 === 0 ? seed + i : i % 3 === 1 ? 'str' + seed + '_' + i : (seed + i) / 4 + 0.5;
        return object;
    }
    const rows = [];
    for (let i = 0; i < 40; i++)
        rows.push(makeWide(20 + (i % 15), i));
    check('big wide', rows);
    check('big wide 16-bit', [...rows, '中']);
}
