// Differential test for the Structure cache in JSON.stringify's fast path. The first object with a given Structure is
// serialized the ordinary way, and later objects with the same Structure reuse its key text and read their values
// straight from storage. Every case below serializes several objects of one shape. The reference output comes from
// the general Stringifier, which we force with an identity replacer.

const spaces = [undefined, 2, '\t'];

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

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

shouldBe(JSON.stringify([{ a: undefined, b: 1 }, { a: undefined, b: 2 }]), '[{"b":1},{"b":2}]');
shouldBe(JSON.stringify([{ a: 1, b: 2 }, { a: undefined, b: 3 }, { a: 4, b: undefined }, { a: undefined, b: undefined }]), '[{"a":1,"b":2},{"b":3},{"a":4},{}]');
shouldBe(JSON.stringify([{ a: 1, b: 2 }, { a: 3, b: 4 }], null, 1), '[\n {\n  "a": 1,\n  "b": 2\n },\n {\n  "a": 3,\n  "b": 4\n }\n]');

// An undefined value in the first, a middle, the last, or every property, whether or not the first object of the
// Structure has it.
function makeMasked(keys, mask, seed) {
    const object = {};
    for (let i = 0; i < keys.length; i++)
        object[keys[i]] = (mask >> i) & 1 ? undefined : seed * 100 + i;
    return object;
}

for (let firstMask = 0; firstMask < 8; firstMask++) {
    const keys = ['a', 'b', 'c'];
    const list = [makeMasked(keys, firstMask, 9)];
    for (let mask = 0; mask < 8; mask++) {
        list.push(makeMasked(keys, mask, mask));
        list.push(makeMasked(keys, mask, mask + 10));
    }
    check(`undefined mask ${firstMask}`, list);
    check(`undefined mask ${firstMask} nested`, { items: list, again: [list, { items: list }] });
}

// Objects of the same Structure nested inside each other.
function makeTree(depth, i) {
    if (!depth)
        return { value: i, children: [] };
    return { value: i, children: [makeTree(depth - 1, i + 1), makeTree(depth - 1, i + 2)] };
}

function makeChain(length) {
    let object = { next: null, value: 0 };
    for (let i = 1; i < length; i++)
        object = { next: object, value: i };
    return object;
}

check('tree', makeTree(5, 0));
check('trees', [makeTree(3, 0), makeTree(2, 1), makeTree(0, 2)]);
check('chains', [makeChain(1), makeChain(5), makeChain(20), { next: makeChain(3), value: 'x' }]);

// Key lengths below, at, and above the SIMD stride of both character widths (16 and 8).
for (const length of [0, 1, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33]) {
    const key = 'x'.repeat(length);
    const rows = [];
    for (let i = 0; i < 4; i++)
        rows.push({ [key]: i, y: i * 2, [key + 'z']: 'v' + i });
    check(`key length ${length}`, rows);
    check(`key length ${length} nested`, { rows, inner: { [key]: rows } });
    // A 16-bit string anywhere makes the output 16-bit.
    check(`key length ${length} 16-bit`, [...rows, '中']);
    check(`key length ${length} 16-bit first`, ['中', ...rows]);
}

// Out-of-line properties, more properties than a Structure can cache, and various value types per property.
function makeWide(count, seed) {
    const object = {};
    for (let i = 0; i < count; i++) {
        const kind = i % 4;
        object['p' + i] = kind === 0 ? seed + i : kind === 1 ? 'str' + seed + '_' + i : kind === 2 ? (seed + i) / 4 + 0.5 : i % 8 === 3;
    }
    return object;
}

for (const count of [1, 4, 6, 7, 8, 16, 31, 32, 33, 40]) {
    const rows = [];
    for (let seed = 0; seed < 4; seed++)
        rows.push(makeWide(count, seed));
    check(`wide ${count}`, rows);
    check(`wide ${count} 16-bit`, [...rows, '中']);
    rows[2].added = 1;
    delete rows[3]['p' + (count >> 1)];
    check(`wide ${count} transitioned`, rows);
}

// More distinct Structures than the cache holds: 8 for the static buffer, 128 for the dynamic buffer.
for (const count of [1, 8, 9, 130]) {
    const list = [];
    for (let repeat = 0; repeat < 2; repeat++) {
        for (let s = 0; s < count; s++)
            list.push({ ['s' + s]: repeat, common: s, text: 'repeat ' + repeat + ' with some padding' });
    }
    check(`${count} structures`, list);
    check(`${count} structures 16-bit`, [...list, '中']);
}

// Every kind of value in a property of a shared Structure.
const rope = 'x'.repeat(40) + String(spaces.length);
const values = [
    'plain', 'quo"te', 'back\\slash', 'new\nline', '\u0000control', 'tab\there', '\u007f', 'café', '中文', '😀', '\ud800',
    rope, rope + rope, '', 'a',
    0, -0, 1, -1, 2147483647, -2147483648, 2147483648, 0.5, -0.5, 1e21, 1e-7, 123456789012, NaN, Infinity, -Infinity,
    null, true, false, undefined, function () { }, Symbol('s'), new Date(0), { toJSON() { return 'toJSON'; } }, { toJSON() { return undefined; } },
    [], [1, 2, 3], [{ v: 1 }, { v: 2 }], [[], [[]]], {}, { nested: 1 }, { v: { v: { v: 1 } } }, new Number(3), new String('boxed'), new Boolean(false),
];

for (let i = 0; i < values.length; i++) {
    check(`value ${i}`, [{ v: values[i] }, { v: values[i] }, { v: 1 }, { v: values[i] }]);
    check(`value ${i} after number`, [{ v: 1.5 }, { v: 2 }, { v: values[i] }, { v: 'str' }]);
    check(`value ${i} with siblings`, [{ a: 1, v: values[i], b: 'two' }, { a: 3, v: values[i], b: 'four' }, { a: 5, v: values[i], b: 6.5 }]);
}

// All of the values interleaved through one Structure.
{
    const rows = [];
    for (let round = 0; round < 2; round++) {
        for (let i = 0; i < values.length; i++)
            rows.push({ a: values[i], b: values[(i + round + 1) % values.length], c: i });
    }
    check('all values', rows);
    check('all values nested', { rows, rows2: [rows] });
}

// Objects whose last property holds a wider object, so that serializing the first one fills the scratch space used to
// record properties of the Structures being serialized. The innermost Structures aren't cached on the first visit and
// are cached on a later one.
function makeNestedWide(level, depth, seed) {
    const object = {};
    for (let i = 0; i < 31; i++)
        object[`l${level}_${i}`] = i % 2 ? seed + i : 'v' + seed + '_' + i;
    object[`l${level}_last`] = depth > 1 ? makeNestedWide(level + 1, depth - 1, seed + 1) : seed;
    return object;
}

for (const depth of [2, 3, 5]) {
    const trees = [];
    for (let seed = 0; seed < 4; seed++)
        trees.push(makeNestedWide(0, depth, seed));
    check(`nested wide ${depth}`, trees);
    check(`nested wide ${depth} nested`, { trees, again: [trees] });
    check(`nested wide ${depth} 16-bit`, [...trees, '中']);
}

// Properties that make a Structure ineligible, mixed in with ones that are eligible.
{
    const hidden = { a: 1 };
    Object.defineProperty(hidden, 'hidden', { value: 2, enumerable: false });
    const withSymbol = { a: 1 };
    withSymbol[Symbol('s')] = 2;
    const withGetter = { a: 1, get b() { return 3; } };
    const withIndex = { 0: 'zero', a: 1 };
    const withEscapedKey = { 'k"ey': 1, 'new\nline': 2 };
    const withWideKey = { 'キー': 1, a: 2 };
    const hiddenToJSON = { a: 1 };
    Object.defineProperty(hiddenToJSON, 'toJSON', { value() { return 'hidden toJSON'; }, enumerable: false });
    const enumerableToJSON = { a: 1, toJSON: 5 };
    const nullPrototype = Object.create(null);
    nullPrototype.a = 1;
    const customPrototype = Object.create({ inherited: 1 });
    customPrototype.a = 1;
    const specials = [hidden, withSymbol, withGetter, withIndex, withEscapedKey, withWideKey, hiddenToJSON, enumerableToJSON, nullPrototype, customPrototype];
    const plain = () => ({ a: 1 });
    for (let i = 0; i < specials.length; i++) {
        check(`special ${i}`, [plain(), plain(), specials[i], plain(), specials[i], plain()]);
        check(`special ${i} first`, [specials[i], plain(), plain(), specials[i], plain()]);
    }
    check('specials', [plain(), ...specials, plain(), ...specials, plain()]);
}
