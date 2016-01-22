description("This test checks the behavior of Array.prototype.toLocaleString as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

shouldBe("Array.prototype.toLocaleString.length", "0");
shouldBeFalse("Object.getOwnPropertyDescriptor(Array.prototype, 'toLocaleString').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype, 'toLocaleString').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype, 'toLocaleString').writable");

// Test toObject abrupt completion.
shouldThrow("Array.prototype.toLocaleString.call()", "'TypeError: undefined is not an object (evaluating \\'Array.prototype.toLocaleString.call()\\')'");
shouldThrow("Array.prototype.toLocaleString.call(undefined)", "'TypeError: undefined is not an object (evaluating \\'Array.prototype.toLocaleString.call(undefined)\\')'");
shouldThrow("Array.prototype.toLocaleString.call(null)", "'TypeError: null is not an object (evaluating \\'Array.prototype.toLocaleString.call(null)\\')'");

// Test Generic invocation.
shouldBeEqualToString("Array.prototype.toLocaleString.call({ length: 5, 0: 'zero', 1: 1, 3: 'three', 5: 'five' })", "zero,1,,three,")

// Empty array is always an empty string.
shouldBeEqualToString("[].toLocaleString()", "");

// Missing still get a separator.
shouldBeEqualToString("Array(5).toLocaleString()", ",,,,");
shouldBeEqualToString("[ null, null ].toLocaleString()", ",");
shouldBeEqualToString("[ undefined, undefined ].toLocaleString()", ",");

// Test that parameters are passed through properly.
shouldThrow("[ new Date ].toLocaleString('i')");
shouldBeEqualToString("[ new Date(NaN), new Date(0) ].toLocaleString('zh-Hans-CN-u-nu-hanidec', { timeZone: 'UTC' })", "Invalid Date,一九七〇/一/一 上午一二:〇〇:〇〇");
