function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
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

shouldBe(Date.prototype.toLocaleString.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleString').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleString').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleString').writable, true);

// Test thisTimeValue abrupt completion.
shouldNotThrow(() => Date.prototype.toLocaleString.call(new Date));
shouldThrow(() => Date.prototype.toLocaleString.call(), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call(undefined), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call(null), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call(0), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call(NaN), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call(Infinity), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call('1'), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call({}), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call([]), TypeError);
shouldThrow(() => Date.prototype.toLocaleString.call(Symbol()), TypeError);

shouldBe(typeof new Date().toLocaleString(), 'string');

shouldBe(new Date(NaN).toLocaleString(), "Invalid Date");

// Test for DateTimeFormat behavior.
// Test that locale parameter is passed through properly.
shouldThrow(() => new Date().toLocaleString('i'), RangeError);
shouldBeOneOf(new Date(0).toLocaleString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' }), [
    '一九七〇/一/一 〇:〇〇:〇〇',
    '一九七〇/一/一 〇〇:〇〇:〇〇',
    '一九七〇/一/一 上午一二:〇〇:〇〇'
]);
shouldBeOneOf(new Date(0).toLocaleString('zh-Hans-CN', { timeZone: 'UTC', numberingSystem: 'hanidec' }), [
    '一九七〇/一/一 〇:〇〇:〇〇',
    '一九七〇/一/一 〇〇:〇〇:〇〇',
    '一九七〇/一/一 上午一二:〇〇:〇〇'
]);
shouldBe(new Date(0).toLocaleString('en-u-ca-chinese', { timeZone: 'UTC', year: 'numeric' }), '1969(ji-you)');
shouldBe(new Date(0).toLocaleString('en', { timeZone: 'UTC', year: 'numeric', calendar: 'chinese' }), '1969(ji-you)');

// Defaults to mdy, hms
shouldBe(new Date(0).toLocaleString('en', { timeZone: 'UTC' }), '1/1/1970, 12:00:00 AM');

// Test that options parameter is passed through properly.
shouldThrow(() => new Date(0).toLocaleString('en', null), TypeError);
shouldBe(new Date(0).toLocaleString('en', { timeZone: 'UTC', hour: 'numeric', minute: '2-digit' }), '12:00 AM');
shouldBe(new Date(0).toLocaleString('en', { timeZone: 'UTC', year: 'numeric', month: 'long' }), 'January 1970');

// Test toLocaleDateString()
shouldBe(Date.prototype.toLocaleDateString.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleDateString').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleDateString').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleDateString').writable, true);

// Test thisTimeValue abrupt completion.
shouldNotThrow(() => Date.prototype.toLocaleDateString.call(new Date));
shouldThrow(() => Date.prototype.toLocaleDateString.call(), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call(undefined), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call(null), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call(0), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call(NaN), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call(Infinity), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call('1'), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call({}), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call([]), TypeError);
shouldThrow(() => Date.prototype.toLocaleDateString.call(Symbol()), TypeError);

shouldBe(typeof new Date().toLocaleDateString(), 'string');

shouldBe(new Date(NaN).toLocaleDateString(), 'Invalid Date');

// Test for DateTimeFormat behavior.
// Test that locale parameter is passed through properly.
shouldThrow(() => new Date().toLocaleDateString('i'), RangeError);
shouldBe(new Date(0).toLocaleDateString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' }), '一九七〇/一/一');
shouldBe(new Date(0).toLocaleDateString('zh-Hans-CN', { timeZone: 'UTC', numberingSystem: 'hanidec' }), '一九七〇/一/一');
shouldBe(new Date(0).toLocaleDateString('en-u-ca-chinese', { timeZone: 'UTC', year: 'numeric' }), '1969(ji-you)');
shouldBe(new Date(0).toLocaleDateString('en', { timeZone: 'UTC', year: 'numeric', calendar: 'chinese' }), '1969(ji-you)');

// Defaults to mdy
shouldBe(new Date(0).toLocaleDateString('en', { timeZone: 'UTC' }), '1/1/1970');

// Test that options parameter is passed through properly.
shouldThrow(() => new Date(0).toLocaleDateString('en', null), TypeError);
// Adds mdy if no date formats specified.
shouldBe(new Date(0).toLocaleDateString('en', { timeZone: 'UTC', hour: 'numeric', minute: '2-digit' }), '1/1/1970, 12:00 AM');
// If any date formats specified, just use them.
shouldBe(new Date(0).toLocaleDateString('en', { timeZone: 'UTC', year: 'numeric', month: 'long' }), 'January 1970');

// Test toLocaleTimeString()
shouldBe(Date.prototype.toLocaleTimeString.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleTimeString').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleTimeString').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleTimeString').writable, true);

// Test thisTimeValue abrupt completion.
shouldNotThrow(() => Date.prototype.toLocaleTimeString.call(new Date));
shouldThrow(() => Date.prototype.toLocaleTimeString.call(), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call(undefined), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call(null), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call(0), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call(NaN), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call(Infinity), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call('1'), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call({}), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call([]), TypeError);
shouldThrow(() => Date.prototype.toLocaleTimeString.call(Symbol()), TypeError);

shouldBe(typeof new Date().toLocaleTimeString(), 'string');

shouldBe(new Date(NaN).toLocaleTimeString(), 'Invalid Date');

// Test for DateTimeFormat behavior.
// Test that locale parameter is passed through properly.
shouldThrow(() => new Date().toLocaleTimeString('i'), RangeError);
shouldBeOneOf(new Date(0).toLocaleTimeString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' }), [ "〇:〇〇:〇〇", "〇〇:〇〇:〇〇", "上午一二:〇〇:〇〇" ]);
shouldBeOneOf(new Date(0).toLocaleTimeString('zh-Hans-CN', { timeZone: 'UTC', numberingSystem: 'hanidec' }), [ "〇:〇〇:〇〇", "〇〇:〇〇:〇〇", "上午一二:〇〇:〇〇" ]);
shouldBe(new Date(0).toLocaleTimeString('en-u-ca-chinese', { timeZone: 'UTC', year: 'numeric' }), '1969(ji-you), 12:00:00 AM');
shouldBe(new Date(0).toLocaleTimeString('en', { timeZone: 'UTC', year: 'numeric', calendar: 'chinese' }), '1969(ji-you), 12:00:00 AM');

// Defaults to hms
shouldBe(new Date(0).toLocaleTimeString('en', { timeZone: 'UTC' }), "12:00:00 AM");

// Test that options parameter is passed through properly.
shouldThrow(() => new Date(0).toLocaleTimeString('en', null), TypeError);
// If time formats specifed, just use them.
shouldBe(new Date(0).toLocaleTimeString('en', { timeZone: 'UTC', hour: 'numeric', minute: '2-digit' }), '12:00 AM');
// Adds hms if no time formats specified.
// See https://bugs.webkit.org/show_bug.cgi?id=238852
const monthLongTimeString = new Date(0).toLocaleTimeString('en', { timeZone: 'UTC', year: 'numeric', month: 'long' });
if (monthLongTimeString !== 'January 1970, 12:00:00 AM' &&
    monthLongTimeString !== 'January 1970 at 12:00:00 AM')
    throw new Error(`Unexpected time string for {month: 'long'}: ${monthLongTimeString}`);
