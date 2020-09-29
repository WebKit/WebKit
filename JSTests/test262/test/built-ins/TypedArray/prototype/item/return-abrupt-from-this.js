// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  Return abrupt from ToObject(this value).
info: |
  %TypedArray%.prototype.item( index )

  Let O be the this value.
  Perform ? ValidateTypedArray(O).

includes: [testTypedArray.js]
features: [TypedArray,TypedArray.prototype.item]
---*/
assert.sameValue(
  typeof TypedArray.prototype.item,
  'function',
  'The value of `typeof TypedArray.prototype.item` is "function"'
);

assert.throws(TypeError, () => {
  TypedArray.prototype.item.call(undefined);
}, '`TypedArray.prototype.item.call(undefined)` throws TypeError');

assert.throws(TypeError, () => {
  TypedArray.prototype.item.call(null);
}, '`TypedArray.prototype.item.call(null)` throws TypeError');

testWithTypedArrayConstructors(TA => {
  assert.sameValue(typeof TA.prototype.item, 'function', 'The value of `typeof TA.prototype.item` is "function"');

  assert.throws(TypeError, () => {
    TA.prototype.item.call(undefined);
  }, '`TA.prototype.item.call(undefined)` throws TypeError');

  assert.throws(TypeError, () => {
    TA.prototype.item.call(null);
  }, '`TA.prototype.item.call(null)` throws TypeError');
});
