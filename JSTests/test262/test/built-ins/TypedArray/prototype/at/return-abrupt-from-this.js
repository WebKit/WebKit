// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.at
description: >
  Return abrupt from ToObject(this value).
info: |
  %TypedArray%.prototype.at( index )

  Let O be the this value.
  Perform ? ValidateTypedArray(O).

includes: [testTypedArray.js]
features: [TypedArray,TypedArray.prototype.at]
---*/
assert.sameValue(
  typeof TypedArray.prototype.at,
  'function',
  'The value of `typeof TypedArray.prototype.at` is "function"'
);

assert.throws(TypeError, () => {
  TypedArray.prototype.at.call(undefined);
}, '`TypedArray.prototype.at.call(undefined)` throws TypeError');

assert.throws(TypeError, () => {
  TypedArray.prototype.at.call(null);
}, '`TypedArray.prototype.at.call(null)` throws TypeError');

testWithTypedArrayConstructors(TA => {
  assert.sameValue(typeof TA.prototype.at, 'function', 'The value of `typeof TA.prototype.at` is "function"');

  assert.throws(TypeError, () => {
    TA.prototype.at.call(undefined);
  }, '`TA.prototype.at.call(undefined)` throws TypeError');

  assert.throws(TypeError, () => {
    TA.prototype.at.call(null);
  }, '`TA.prototype.at.call(null)` throws TypeError');
});
