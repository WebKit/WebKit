//@ skip if $hostOS == "windows"
description("This test checks the behavior of Number.prototype.toLocaleString as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

shouldBe("Number.prototype.toLocaleString.length", "0");
shouldBeFalse("Object.getOwnPropertyDescriptor(Number.prototype, 'toLocaleString').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Number.prototype, 'toLocaleString').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Number.prototype, 'toLocaleString').writable");

// Test thisNumberValue abrupt completion.
shouldNotThrow("Number.prototype.toLocaleString.call(0)");
shouldNotThrow("Number.prototype.toLocaleString.call(NaN)");
shouldNotThrow("Number.prototype.toLocaleString.call(Infinity)");
shouldNotThrow("Number.prototype.toLocaleString.call(new Number)");
shouldThrow("Number.prototype.toLocaleString.call()", "'TypeError: Number.prototype.toLocaleString called on incompatible undefined'");
shouldThrow("Number.prototype.toLocaleString.call(undefined)", "'TypeError: Number.prototype.toLocaleString called on incompatible undefined'");
shouldThrow("Number.prototype.toLocaleString.call(null)", "'TypeError: Number.prototype.toLocaleString called on incompatible object'");
shouldThrow("Number.prototype.toLocaleString.call('1')", "'TypeError: Number.prototype.toLocaleString called on incompatible string'");
shouldThrow("Number.prototype.toLocaleString.call([])", "'TypeError: Number.prototype.toLocaleString called on incompatible object'");
shouldThrow("Number.prototype.toLocaleString.call(Symbol())", "'TypeError: Number.prototype.toLocaleString called on incompatible symbol'");

shouldBeEqualToString("(0).toLocaleString()", "0");
shouldBeEqualToString("new Number(1).toLocaleString()", "1");

// Test for NumberFormat behavior.
shouldThrow("(0).toLocaleString('i')", "'RangeError: invalid language tag: i'");
shouldBeEqualToString("Infinity.toLocaleString()", "∞");

// Test that locale parameter is passed through properly.
shouldBeEqualToString("(123456.789).toLocaleString('ar')", "١٢٣٬٤٥٦٫٧٨٩");
shouldBeEqualToString("(123456.789).toLocaleString('zh-Hans-CN-u-nu-hanidec')", "一二三,四五六.七八九");

// Test that options parameter is passed through properly.
shouldBeEqualToString("(123.456).toLocaleString('en', { maximumSignificantDigits: 3 })", "123");
