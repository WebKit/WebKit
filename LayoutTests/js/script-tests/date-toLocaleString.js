//@ skip if $hostOS == "windows"
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

// Test toLocaleDateString ()
shouldBe("Date.prototype.toLocaleDateString.length", "0");
shouldBeFalse("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleDateString').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleDateString').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleDateString').writable");

// Test thisTimeValue abrupt completion.
shouldNotThrow("Date.prototype.toLocaleDateString.call(new Date)");
shouldThrow("Date.prototype.toLocaleDateString.call()");
shouldThrow("Date.prototype.toLocaleDateString.call(undefined)");
shouldThrow("Date.prototype.toLocaleDateString.call(null)");
shouldThrow("Date.prototype.toLocaleDateString.call(0)");
shouldThrow("Date.prototype.toLocaleDateString.call(NaN)");
shouldThrow("Date.prototype.toLocaleDateString.call(Infinity)");
shouldThrow("Date.prototype.toLocaleDateString.call('1')");
shouldThrow("Date.prototype.toLocaleDateString.call({})");
shouldThrow("Date.prototype.toLocaleDateString.call([])");
shouldThrow("Date.prototype.toLocaleDateString.call(Symbol())");

shouldBeTrue("typeof new Date().toLocaleDateString() === 'string'");

shouldBeEqualToString("new Date(NaN).toLocaleDateString()", "Invalid Date");

// Test for DateTimeFormat behavior.
// Test that locale parameter is passed through properly.
shouldThrow("new Date().toLocaleDateString('i')");
shouldBeEqualToString("new Date(0).toLocaleDateString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' })", "一九七〇/一/一");

// Defaults to mdy
shouldBeEqualToString("new Date(0).toLocaleDateString('en', { timeZone: 'UTC' })", "1/1/1970");

// Test that options parameter is passed through properly.
shouldThrow("new Date(0).toLocaleDateString('en', null)", "'TypeError: null is not an object'");
// Adds mdy if no date formats specified.
shouldBeEqualToString("new Date(0).toLocaleDateString('en', { timeZone: 'UTC', hour:'numeric', minute:'2-digit' })", "1/1/1970, 12:00 AM");
// If any date formats specified, just use them.
shouldBeEqualToString("new Date(0).toLocaleDateString('en', { timeZone: 'UTC', year:'numeric', month:'long' })", "January 1970");

// Test toLocaleTimeString ()
shouldBe("Date.prototype.toLocaleTimeString.length", "0");
shouldBeFalse("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleTimeString').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleTimeString').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Date.prototype, 'toLocaleTimeString').writable");

// Test thisTimeValue abrupt completion.
shouldNotThrow("Date.prototype.toLocaleTimeString.call(new Date)");
shouldThrow("Date.prototype.toLocaleTimeString.call()");
shouldThrow("Date.prototype.toLocaleTimeString.call(undefined)");
shouldThrow("Date.prototype.toLocaleTimeString.call(null)");
shouldThrow("Date.prototype.toLocaleTimeString.call(0)");
shouldThrow("Date.prototype.toLocaleTimeString.call(NaN)");
shouldThrow("Date.prototype.toLocaleTimeString.call(Infinity)");
shouldThrow("Date.prototype.toLocaleTimeString.call('1')");
shouldThrow("Date.prototype.toLocaleTimeString.call({})");
shouldThrow("Date.prototype.toLocaleTimeString.call([])");
shouldThrow("Date.prototype.toLocaleTimeString.call(Symbol())");

shouldBeTrue("typeof new Date().toLocaleTimeString() === 'string'");

shouldBeEqualToString("new Date(NaN).toLocaleTimeString()", "Invalid Date");

// Test for DateTimeFormat behavior.
// Test that locale parameter is passed through properly.
shouldThrow("new Date().toLocaleTimeString('i')");
shouldBeEqualToString("new Date(0).toLocaleTimeString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' })", "上午一二:〇〇:〇〇");

// Defaults to hms
shouldBeEqualToString("new Date(0).toLocaleTimeString('en', { timeZone: 'UTC' })", "12:00:00 AM");

// Test that options parameter is passed through properly.
shouldThrow("new Date(0).toLocaleTimeString('en', null)", "'TypeError: null is not an object'");
// If time formats specifed, just use them.
shouldBeEqualToString("new Date(0).toLocaleTimeString('en', { timeZone: 'UTC', hour:'numeric', minute:'2-digit' })", "12:00 AM");
// Adds hms if no time formats specified.
shouldBeEqualToString("new Date(0).toLocaleTimeString('en', { timeZone: 'UTC', year:'numeric', month:'long' })", "January 1970, 12:00:00 AM");
