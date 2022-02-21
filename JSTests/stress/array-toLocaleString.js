function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

shouldBe(Array.prototype.toLocaleString.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Array.prototype, 'toLocaleString').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Array.prototype, 'toLocaleString').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Array.prototype, 'toLocaleString').writable, true);

// Test toObject abrupt completion.
shouldThrow(() => Array.prototype.toLocaleString.call(), TypeError);
shouldThrow(() => Array.prototype.toLocaleString.call(undefined), TypeError);
shouldThrow(() => Array.prototype.toLocaleString.call(null), TypeError);

// Test Generic invocation.
shouldBe(Array.prototype.toLocaleString.call({ length: 5, 0: 'zero', 1: 1, 3: 'three', 5: 'five' }), 'zero,1,,three,')

// Empty array is always an empty string.
shouldBe([].toLocaleString(), '');

// Missing still get a separator.
shouldBe(Array(5).toLocaleString(), ',,,,');
shouldBe([ null, null ].toLocaleString(), ',');
shouldBe([ undefined, undefined ].toLocaleString(), ',');

// Test that parameters are passed through properly.
shouldThrow(() => [ new Date ].toLocaleString('i'), RangeError);
shouldBe([ new Date(NaN), new Date(0) ].toLocaleString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' }), $vm.icuVersion() >= 69 ? 'Invalid Date,一九七〇/一/一 〇〇:〇〇:〇〇' : 'Invalid Date,一九七〇/一/一 上午一二:〇〇:〇〇');
