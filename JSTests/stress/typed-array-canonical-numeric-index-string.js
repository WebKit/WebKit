//@ requireOptions("--forceEagerCompilation=true", "--osrExitCountForReoptimization=10", "--useConcurrentJIT=0")

const typedArrays = [
    Uint8ClampedArray,
    Uint8Array,
    Uint16Array,
    Uint32Array,
    Int8Array,
    Int16Array,
    Int32Array,
    Float32Array,
    Float64Array,
];

const failures = new Set();

function makeTest(test) {
    noInline(test);

    function assert(typedArray, condition, message) {
        if (!condition)
            failures.add(`${typedArray.name}: ${message}`);
    }

    function testFor(typedArray, key) {
        key = JSON.stringify(key);
        return new Function('test', 'assert', `
            const value = 42;
            const u8 = new ${typedArray.name}(1);
            u8[${key}] = value;
            test(u8, ${key}, value, assert.bind(undefined, ${typedArray.name}));
        `).bind(undefined, test, assert);
    };

    return function(keys) {
        for (let typedArray of typedArrays) {
            for (let key of keys) {
                const runTest = testFor(typedArray, key);
                noInline(runTest);
                for (let i = 0; i < 10; i++) {
                    runTest();
                }
            }
        }
    }
}

const testInvalidIndices = makeTest((array, key, value, assert) => {
    assert(array[key] === undefined, `${key} should not be set`);
    assert(!(key in array), `${key} should not be in array`);

    const keys = Object.keys(array);
    assert(keys.length === 1, `no new keys should be added`);
    assert(keys[0] === '0', `'0' should be the only key`);
    assert(array[0] === 0, `offset 0 should not have been modified`);
});

testInvalidIndices([
    '-0',
    '-1',
    -1,
    1,
    'Infinity',
    '-Infinity',
    'NaN',
    '0.1',
    '4294967294',
    '4294967295',
    '4294967296',
]);

const testValidIndices = makeTest((array, key, value, assert) => {
    assert(array[key] === value, `${key} should be set to ${value}`);
    assert(key in array, `should contain key ${key}`);
});

testValidIndices([
    '01',
    '0.10',
    '+Infinity',
    '-NaN',
    '-0.0',
    '0',
    0,
]);

if (failures.size)
    throw new Error(`Subtests failed:\n${Array.from(failures).join('\n')}`);
