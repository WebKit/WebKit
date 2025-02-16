// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 918879;
var summary = 'String.fromCodePoint';

print(BUGNUMBER + ": " + summary);

// Tests taken from:
// https://github.com/mathiasbynens/String.fromCodePoint/blob/master/tests/tests.js

assert.sameValue(String.fromCodePoint.length, 1);
assert.sameValue(String.fromCodePoint.name, 'fromCodePoint');
assert.sameValue(String.propertyIsEnumerable('fromCodePoint'), false);

assert.sameValue(String.fromCodePoint(''), '\0');
assert.sameValue(String.fromCodePoint(), '');
assert.sameValue(String.fromCodePoint(-0), '\0');
assert.sameValue(String.fromCodePoint(0), '\0');
assert.sameValue(String.fromCodePoint(0x1D306), '\uD834\uDF06');
assert.sameValue(String.fromCodePoint(0x1D306, 0x61, 0x1D307), '\uD834\uDF06a\uD834\uDF07');
assert.sameValue(String.fromCodePoint(0x61, 0x62, 0x1D307), 'ab\uD834\uDF07');
assert.sameValue(String.fromCodePoint(false), '\0');
assert.sameValue(String.fromCodePoint(null), '\0');

assertThrowsInstanceOf(function() { String.fromCodePoint('_'); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint('+Infinity'); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint('-Infinity'); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(-1); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(0x10FFFF + 1); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(3.14); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(3e-2); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(Infinity); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(NaN); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(undefined); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint({}); }, RangeError);

var counter = Math.pow(2, 15) * 3 / 2;
var result = [];
while (--counter >= 0) {
        result.push(0); // one code unit per symbol
}
String.fromCodePoint.apply(null, result); // must not throw

var counter = Math.pow(2, 15) * 3 / 2;
var result = [];
while (--counter >= 0) {
        result.push(0xFFFF + 1); // two code units per symbol
}
String.fromCodePoint.apply(null, result); // must not throw

// str_fromCodePoint_one_arg (single argument, creates an inline string)
assert.sameValue(String.fromCodePoint(0x31), '1');
// str_fromCodePoint_few_args (few arguments, creates an inline string)
// JSFatInlineString::MAX_LENGTH_TWO_BYTE / 2 = floor(11 / 2) = 5
assert.sameValue(String.fromCodePoint(0x31, 0x32), '12');
assert.sameValue(String.fromCodePoint(0x31, 0x32, 0x33), '123');
assert.sameValue(String.fromCodePoint(0x31, 0x32, 0x33, 0x34), '1234');
assert.sameValue(String.fromCodePoint(0x31, 0x32, 0x33, 0x34, 0x35), '12345');
// str_fromCodePoint (many arguments, creates a malloc string)
assert.sameValue(String.fromCodePoint(0x31, 0x32, 0x33, 0x34, 0x35, 0x36), '123456');

