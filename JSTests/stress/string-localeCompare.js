function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
  func();
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

shouldBe(String.prototype.localeCompare.length, 1);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').writable, true);

// Test RequireObjectCoercible.
shouldThrow(() => String.prototype.localeCompare.call(), TypeError);
shouldThrow(() => String.prototype.localeCompare.call(undefined), TypeError);
shouldThrow(() => String.prototype.localeCompare.call(null), TypeError);
shouldNotThrow(() => String.prototype.localeCompare.call({}, ''));
shouldNotThrow(() => String.prototype.localeCompare.call([], ''));
shouldNotThrow(() => String.prototype.localeCompare.call(NaN, ''));
shouldNotThrow(() => String.prototype.localeCompare.call(5, ''));
shouldNotThrow(() => String.prototype.localeCompare.call('', ''));
shouldNotThrow(() => String.prototype.localeCompare.call(() => {}, ''));

// Test toString fails.
shouldThrow(() => ''.localeCompare.call({ toString() { throw new Error() } }, ''), Error);
shouldThrow(() => ''.localeCompare({ toString() { throw new Error() } }), Error);
shouldNotThrow(() => ''.localeCompare());
shouldNotThrow(() => ''.localeCompare(null));

// Basic support.
shouldBe('a'.localeCompare('aa'), -1);
shouldBe('a'.localeCompare('b'), -1);

shouldBe('a'.localeCompare('a'), 0);
shouldBe('a\u0308\u0323'.localeCompare('a\u0323\u0308'), 0);

shouldBe('aa'.localeCompare('a'), 1);
shouldBe('b'.localeCompare('a'), 1);

// Uses Intl.Collator.
shouldThrow(() => 'a'.localeCompare('b', '$'), RangeError);
shouldThrow(() => 'a'.localeCompare('b', 'en', {usage: 'Sort'}), RangeError);

shouldBe('ä'.localeCompare('z', 'en'), -1);
shouldBe('ä'.localeCompare('z', 'sv'), 1);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'base' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'base' }), 0);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'base' }), 0);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'base' }), 0);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'accent' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'accent' }), -1);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'accent' }), 0);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'accent' }), 0);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'case' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'case' }), 0);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'case' }), -1);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'case' }), 0);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'variant' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'variant' }), -1);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'variant' }), -1);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'variant' }), -1);

shouldBe('1'.localeCompare('2', 'en', { numeric: false }), -1);
shouldBe('2'.localeCompare('10', 'en', { numeric: false }), 1);
shouldBe('01'.localeCompare('1', 'en', { numeric: false }), -1);
shouldBe('๑'.localeCompare('๒', 'en', { numeric: false }), -1);
shouldBe('๒'.localeCompare('๑๐', 'en', { numeric: false }), 1);
shouldBe('๐๑'.localeCompare('๑', 'en', { numeric: false }), -1);

shouldBe('1'.localeCompare('2', 'en', { numeric: true }), -1);
shouldBe('2'.localeCompare('10', 'en', { numeric: true }), -1);
shouldBe('01'.localeCompare('1', 'en', { numeric: true }), 0);
shouldBe('๑'.localeCompare('๒', 'en', { numeric: true }), -1);
shouldBe('๒'.localeCompare('๑๐', 'en', { numeric: true }), -1);
shouldBe('๐๑'.localeCompare('๑', 'en', { numeric: true }), 0);

