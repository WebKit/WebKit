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
var BUGNUMBER = 1039774;
var summary = 'String.raw';

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOf(function() { String.raw(); }, TypeError);

assert.sameValue(String.raw.length, 1);

var cooked = [];
assertThrowsInstanceOf(function() { String.raw(cooked); }, TypeError);

cooked.raw = {};
assert.sameValue(String.raw(cooked), "");

cooked.raw = {lengt: 0};
assert.sameValue(String.raw(cooked), "");

cooked.raw = {length: 0};
assert.sameValue(String.raw(cooked), "");

cooked.raw = {length: -1};
assert.sameValue(String.raw(cooked), "");

cooked.raw = [];
assert.sameValue(String.raw(cooked), "");

cooked.raw = ["a"];
assert.sameValue(String.raw(cooked), "a");

cooked.raw = ["a", "b"];
assert.sameValue(String.raw(cooked, "x"), "axb");

cooked.raw = ["a", "b"];
assert.sameValue(String.raw(cooked, "x", "y"), "axb");

cooked.raw = ["a", "b", "c"];
assert.sameValue(String.raw(cooked, "x"), "axbc");

cooked.raw = ["a", "b", "c"];
assert.sameValue(String.raw(cooked, "x", "y"), "axbyc");

cooked.raw = ["\n", "\r\n", "\r"];
assert.sameValue(String.raw(cooked, "x", "y"), "\nx\r\ny\r");

cooked.raw = ["\n", "\r\n", "\r"];
assert.sameValue(String.raw(cooked, "\r\r", "\n"), "\n\r\r\r\n\n\r");

cooked.raw = {length: 2, '0':"a", '1':"b", '2':"c"};
assert.sameValue(String.raw(cooked, "x", "y"), "axb");

cooked.raw = {length: 4, '0':"a", '1':"b", '2':"c"};
assert.sameValue(String.raw(cooked, "x", "y"), "axbycundefined");

