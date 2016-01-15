description("This test checks the behavior of Date.prototype.toLocaleString as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

shouldBe("Date.prototype.toLocaleString.length", "0");
shouldBeFalse("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleString').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleString').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleString').writable");

// Test thisTimeValue abrupt completion.
shouldNotThrow("Date.prototype.toLocaleString.call(new Date)");
shouldThrow("Date.prototype.toLocaleString.call()");
shouldThrow("Date.prototype.toLocaleString.call(undefined)");
shouldThrow("Date.prototype.toLocaleString.call(null)");
shouldThrow("Date.prototype.toLocaleString.call(0)");
shouldThrow("Date.prototype.toLocaleString.call(NaN)");
shouldThrow("Date.prototype.toLocaleString.call(Infinity)");
shouldThrow("Date.prototype.toLocaleString.call('1')");
shouldThrow("Date.prototype.toLocaleString.call({})");
shouldThrow("Date.prototype.toLocaleString.call([])");
shouldThrow("Date.prototype.toLocaleString.call(Symbol())");

shouldBeTrue("typeof new Date().toLocaleString() === 'string'");

shouldBeEqualToString("new Date(NaN).toLocaleString()", "Invalid Date");

// Test for DateTimeFormat behavior.
// Test that locale parameter is passed through properly.
shouldThrow("new Date().toLocaleString('i')");
shouldBeEqualToString("new Date(0).toLocaleString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' })", "一九七〇/一/一 上午一二:〇〇:〇〇");

// Defaults to mdy, hms
shouldBeEqualToString("new Date(0).toLocaleString('en', { timeZone: 'UTC' })", "1/1/1970, 12:00:00 AM");

// Test that options parameter is passed through properly.
shouldThrow("new Date(0).toLocaleString('en', null)", "'TypeError: null is not an object'");
shouldBeEqualToString("new Date(0).toLocaleString('en', { timeZone: 'UTC', hour:'numeric', minute:'2-digit' })", "12:00 AM");
shouldBeEqualToString("new Date(0).toLocaleString('en', { timeZone: 'UTC', year:'numeric', month:'long' })", "January 1970");

