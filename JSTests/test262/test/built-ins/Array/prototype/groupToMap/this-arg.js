// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap calls with thisArg
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
      c. Let key be ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »).

  ...
flags: [noStrict]
features: [array-grouping, Map]
---*/


let sentinel = {};
let result;

[1].groupToMap(function() {
  result = this;
}, "TestString");
assert.sameValue(result instanceof String, true);
assert.sameValue(result.valueOf(), "TestString");

[1].groupToMap(function() {
  result = this;
}, 1);
assert.sameValue(result instanceof Number, true);
assert.sameValue(result.valueOf(), 1);

[1].groupToMap(function() {
  result = this;
}, true);
assert.sameValue(result instanceof Boolean, true);
assert.sameValue(result.valueOf(), true);

[1].groupToMap(function() {
  result = this;
}, null);
assert.sameValue(result, globalThis);

[1].groupToMap(function() {
  result = this;
}, sentinel);
assert.sameValue(result, sentinel);

[1].groupToMap(function() {
  result = this;
}, void 0);
assert.sameValue(result, globalThis);

[1].groupToMap(function() {
  result = this;
});
assert.sameValue(result, globalThis);
